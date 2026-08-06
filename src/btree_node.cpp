#include "../include/btree_node.h"
#include "../include/buffer_pool.h"
#include <cstdint>
#include <utility>
#include <vector>


// Two rules run through this whole file.
//
// First, a node is never allocated, it is a page reinterpreted. So any change to a
// node is a change to the frame, and the page has to be unpinned with dirty = true or
// the edit is thrown away on eviction. Every path out of every function below has to
// unpin exactly what it pinned, which is why the unpin calls look so repetitive.
//
// Second, keys inside a node are kept sorted, and the search is a plain linear scan.
// With up to 509 keys a binary search would be the better answer, but a scan over one
// resident page is cheap next to the page read that got us here, so it has not been
// worth the extra off-by-one risk yet.
namespace Kintsugi::Tree
{
     // Starts every tree as a single empty leaf that is also the root. That is the one
     // case where a node is allowed to sit below MIN_KEYS: a tree with three keys in it
     // has nowhere else to put them.
     BTreeIndex::BTreeIndex(Kintsugi::BufferPool::BufferPoolManager* bpm) : _bpm(bpm)
     {
          BufferPool::Frame* frame = _bpm->new_page(&_root_page_id);
          BTreeNode*         node  = reinterpret_cast<BTreeNode*>(frame->data);

          node->is_leaf           = true;
          node->key_count         = 0;
          node->next_leaf_page_id = -1;

          _bpm->unpin_page(_root_page_id, true);
     }

     // Root to leaf, one page pinned at a time. The parent is released before the
     // child is fetched, so a deep tree still only occupies one frame here.
     bool BTreeIndex::search(int key, int* out_value)
     {
          int current_page_id = _root_page_id;

          while(true)
          {
               BufferPool::Frame* frame = _bpm->fetch_page(current_page_id);
               BTreeNode* node = reinterpret_cast<BTreeNode*>(frame->data);

               if(node->is_leaf)
               {
                    for(std::uint32_t i = 0; i < node->key_count; ++i) 
                    {
                         if(node->keys[i] == key)
                         {
                              *out_value = node->values[i];
                              _bpm->unpin_page(current_page_id, false);
                              return true;
                         }
                    }

                    _bpm->unpin_page(current_page_id, false);
                    return false;
               }
               else
               {
                    // children[i] holds everything below keys[i], and there is one more
                    // child than there are keys. Defaulting to key_count picks that
                    // last child, which is where a key larger than all of them lives.
                    int child_index = node->key_count;
                    for(std::uint32_t i = 0; i < node->key_count; ++i)
                    {
                         if(key < node->keys[i])
                         {
                              child_index = i;
                              break;
                         }
                    }

                    // Read the id out before unpinning. After the unpin the frame can
                    // be handed to somebody else and node points at the wrong page.
                    int next_page_id = node->children[child_index];
                    _bpm->unpin_page(current_page_id, false);
                    current_page_id = next_page_id;
               }
          }
     }

     // Slots the key into a leaf that is already sorted. Does not check for room and
     // does not split: it is allowed to push the node one key over MAX_KEYS, and the
     // caller cleans that up. That is why the arrays have a spare slot.
     void BTreeIndex::insert_into_leaf(BTreeNode* node, int key, int value)
     {
          int insert_pos = node->key_count;
          for(std::uint32_t i = 0; i < node->key_count; ++i)
          {
               if(key < node->keys[i])
               {
                    insert_pos = i;
                    break;
               }
          }

          // Backwards, so each slot is copied before it gets overwritten. Going
          // forwards would smear the first value across the rest of the array.
          for(std::uint32_t i = node->key_count; i > insert_pos; --i)
          {
               node->keys[i]   = node->keys[i - 1];
               node->values[i] = node->values[i - 1];
          }

          node->keys[insert_pos]   = key;
          node->values[insert_pos] = value;

          node->key_count++;
     }

     // Same idea one level up: a child split, so the parent takes the key that came up
     // out of the split plus a pointer to the new right half. The new child always goes
     // to the right of the key, because the left half kept the page id it already had.
     void BTreeIndex::insert_into_internal(BTreeNode* node, int middle_key, int right_page_id)
     {
          int insert_pos = node->key_count;

          for(std::uint32_t i = 0; i < node->key_count; ++i)
          {
               if(middle_key < node->keys[i])
               {
                    insert_pos = i;
                    break;
               }
          }

          for(std::uint32_t i = node->key_count; i > insert_pos; --i)
          {
               node->keys[i] = node->keys[i - 1];
          }

          node->keys[insert_pos] = middle_key;

          // Children start one index later than keys and there is one extra of them,
          // hence key_count + 1 and insert_pos + 1 instead of the plain versions above.
          for(std::uint32_t i = node->key_count + 1; i >insert_pos + 1; --i)
          {
               node->children[i] = node->children[i - 1];
          }

          node->children[insert_pos + 1] = right_page_id;
          node->key_count++;
     }

     // Cuts a full leaf in half. The upper half moves to a brand new page, the lower
     // half stays where it is, so the parent's existing pointer to this leaf is still
     // right and only the new page needs announcing.
     //
     // left_page_id is not used. The chain relink below works off the node itself, and
     // the parameter is only still here because the caller has the id handy.
     int BTreeIndex::split_leaf(int left_page_id, BTreeNode* left, int* out_middle_key)
     {
          int right_page_id;

          BufferPool::Frame* right_frame = _bpm->new_page(&right_page_id);
          BTreeNode*         right       = reinterpret_cast<BTreeNode*>(right_frame->data);

          int middle_index = left->key_count / 2;

          for(std::uint32_t i = middle_index; i < left->key_count; ++i)
          {
               right->keys[i - middle_index] = left->keys[i];
               right->values[i - middle_index] = left->values[i];
          }

          right->key_count          = left->key_count - middle_index;
          left->key_count           = middle_index;

          // Splice the new page into the leaf chain, right first. Do it the other way
          // round and left->next is already overwritten when we go to read it.
          right->next_leaf_page_id  = left->next_leaf_page_id;
          left->next_leaf_page_id   = right_page_id;
          right->is_leaf            = true;

          // Copied up, not moved up. A leaf holds real data, so the first key of the
          // right half has to stay in the right half as well as guide the parent.
          *out_middle_key = right->keys[0];

          _bpm->unpin_page(right_page_id, true);

          return right_page_id;
     }

     // The internal version, and the one real difference from split_leaf: here the
     // middle key moves up instead of being copied. An internal key is only a
     // signpost, and the child it points into ends up under the parent anyway, so
     // keeping a copy would mean the same key sitting at two levels.
     int BTreeIndex::split_internal(BTreeNode* left, int* out_middle_key)
     {
          int right_page_id;
          
          BufferPool::Frame* right_frame = _bpm->new_page(&right_page_id);
          BTreeNode* right = reinterpret_cast<BTreeNode*>(right_frame->data);

          right->is_leaf = false;

          int middle_index = left->key_count / 2;
          *out_middle_key  = left->keys[middle_index];

          int right_key_count = 0;
          for(std::uint32_t i = middle_index + 1; i < left->key_count; ++i)
          {
               right->keys[right_key_count++] = left->keys[i];
          }

          right->key_count = right_key_count;

          // Note the <=: n keys mean n + 1 children, so the last child has to come
          // across too or the right half loses a subtree.
          int right_child_count = 0;
          for(std::uint32_t i = middle_index + 1; i <= left->key_count; ++i)
          {
               right->children[right_child_count++] = left->children[i];
          }

          // Dropping key_count is all it takes to give up the moved keys. The old
          // values are still sitting in the array, just out of reach now.
          left->key_count = middle_index;

          _bpm->unpin_page(right_page_id, true);
          return right_page_id;
     }

     // Down to the leaf, insert, and then let the splits climb back up as far as they
     // need to. In the normal case nothing splits and it stops at the leaf.
     bool BTreeIndex::insert(int key, int value)
     {
          // Duplicates are refused. Costs a second descent, but it keeps the split
          // logic from ever having to think about two equal keys.
          int n;
          if(search(key, &n)) return false;

          // Breadcrumbs. There are no parent pointers in a node, so the way back up
          // has to be remembered on the way down.
          std::vector<int> path;

          int current_page_id = _root_page_id;

          BufferPool::Frame* frame = _bpm->fetch_page(current_page_id);
          BTreeNode* node  = reinterpret_cast<BTreeNode*>(frame->data);

          while(!node->is_leaf) 
          {
               path.push_back(current_page_id);
               int child_index = node->key_count;

               for(std::uint32_t i = 0; i < node->key_count; ++i)
               {
                    if(key < node->keys[i])
                    {
                         child_index = i;
                         break;
                    }
               }

               int next_page_id = node->children[child_index];
               _bpm->unpin_page(current_page_id, false);
               current_page_id = next_page_id;
               
               frame = _bpm->fetch_page(current_page_id);
               node = reinterpret_cast<BTreeNode*>(frame->data);
          }

          insert_into_leaf(node, key, value);

          // Still within limits, so the easy way out. This is what happens almost
          // every time.
          if(node->key_count <= BTreeNode::MAX_KEYS)
          {
               _bpm->unpin_page(current_page_id, true);
               return true;
          }

          int middle_key;
          int right_page_id = split_leaf(current_page_id, node, &middle_key);
          _bpm->unpin_page(current_page_id, true);

          int left_page_id = current_page_id;

          // Push the split up the breadcrumb trail. Each parent that swallows the new
          // key without overflowing ends the loop. A parent that overflows splits too,
          // and its own middle key becomes the next thing to push up.
          while(!path.empty())
          {
               int parent_page_id = path.back();
               path.pop_back();

               BufferPool::Frame* parent_frame = _bpm->fetch_page(parent_page_id);
               BTreeNode*         parent       = reinterpret_cast<BTreeNode*>(parent_frame->data);

               insert_into_internal(parent, middle_key, right_page_id);

               if(parent->key_count <= BTreeNode::MAX_KEYS)
               {
                    _bpm->unpin_page(parent_page_id, true);
                    return true;
               }

               int new_middle_key;
               int new_right_page_id = split_internal(parent, &new_middle_key);
               _bpm->unpin_page(parent_page_id, true);

               middle_key    = new_middle_key;
               right_page_id = new_right_page_id;
               left_page_id  = parent_page_id;
          }

          // Fell out of the loop, so the split made it all the way past the old root.
          // A new root goes on top with a single key and the two halves under it, and
          // this is the only thing that ever makes the tree taller.
          int new_root_page_id;
          BufferPool::Frame* new_root_frame = _bpm->new_page(&new_root_page_id);
          BTreeNode* new_root = reinterpret_cast<BTreeNode*>(new_root_frame->data);

          new_root->is_leaf     = false;
          new_root->key_count   = 1;
          new_root->keys[0]     = middle_key;
          new_root->children[0] = left_page_id;
          new_root->children[1] = right_page_id;

          _root_page_id = new_root_page_id;
          _bpm->unpin_page(new_root_page_id, true);

          return true;
     }

     // Descend once to the leaf holding min, then follow next_leaf_page_id sideways
     // until a key goes past max. This is what the leaf chain exists for: without it
     // every step would have to climb back up through the parents.
     std::vector<std::pair<int, int>> BTreeIndex::range_query(std::uint32_t min, std::uint32_t max) const
     {
          std::vector<std::pair<int, int>> out_vector;

          int current_page_id = _root_page_id;
          BufferPool::Frame* frame = _bpm->fetch_page(current_page_id);
          BTreeNode* node  = reinterpret_cast<BTreeNode*>(frame->data);

          while(!node->is_leaf)
          {
               int child_index = node->key_count;
               for(std::uint32_t i = 0; i < node->key_count; ++i)
               {
                    if(min < node->keys[i])
                    {
                         child_index = i;
                         break;
                    }
               }

               int next_page_id = node->children[child_index];
               _bpm->unpin_page(current_page_id, false);
               current_page_id = next_page_id;
               frame = _bpm->fetch_page(current_page_id);
               node = reinterpret_cast<BTreeNode*>(frame->data);

          }

          while(true)
          {
               for(std::uint32_t i = 0; i < node->key_count; ++i)
               {
                    // Keys are sorted, so the first one past max means every key after
                    // it is too, in this leaf and in all the ones further along.
                    //
                    // The bounds are unsigned while the keys are int, so the key gets
                    // converted here. Safe only because every key in this database is
                    // an id from the keymap and those are never negative.
                    if(node->keys[i] > max)
                    {
                         _bpm->unpin_page(current_page_id, false);
                         return out_vector;
                    }

                    if(node->keys[i] >= min)
                    {
                         out_vector.push_back({node->keys[i], node->values[i]});
                    }
               }

               int next = node->next_leaf_page_id;
               _bpm->unpin_page(current_page_id, false);
               if(next == -1) break;
               current_page_id = next;
               frame = _bpm->fetch_page(current_page_id);
               node = reinterpret_cast<BTreeNode*>(frame->data);
          }

          return out_vector;
     }

     // The awkward one. Taking a key out is easy, keeping the tree balanced afterwards
     // is not, and that is what most of this function is about.
     //
     // Once a node drops under MIN_KEYS there are three ways out, tried in this order:
     //   1. borrow one key from the left sibling
     //   2. borrow one from the right sibling
     //   3. no sibling can spare anything, so merge with one of them
     //
     // Borrowing is preferred because it touches two nodes and stops there. A merge
     // removes a key from the parent, which can push the parent under the limit as
     // well, so the whole thing may have to repeat one level up.
     bool BTreeIndex::delete_k(int key)
     {
          int n;
          if(!search(key, &n)) return false;

          // Page id plus which child we came down through. The index is needed on the
          // way back up to find the siblings and the separator key in the parent.
          std::vector<std::pair<int, int>> path;

          int current_page_id = _root_page_id;
          BufferPool::Frame* frame = _bpm->fetch_page(current_page_id);
          BTreeNode* node = reinterpret_cast<BTreeNode*>(frame->data);

          while(!node->is_leaf)
          {
               int child_index = node->key_count;
               for(std::uint32_t i = 0; i < node->key_count; ++i)
               {
                    if(key < node->keys[i])
                    {
                         child_index = i;
                         break;
                    }
               }

               path.push_back({current_page_id, child_index});

               int next_page_id = node->children[child_index];
               _bpm->unpin_page(current_page_id, false);

               current_page_id = next_page_id;
               frame = _bpm->fetch_page(current_page_id);
               node = reinterpret_cast<BTreeNode*>(frame->data);
          }

          std::uint32_t delete_pos = node->key_count;
          for(std::uint32_t i = 0; i < node->key_count; ++i)
          {
               if(node->keys[i] == key)
               {
                    delete_pos = i;
                    break;
               }
          }

          //not found
          if(delete_pos == node->key_count)
          {
               _bpm->unpin_page(current_page_id, false);
               return false;
          }

          // Close the gap by sliding everything after it down one slot. The stale copy
          // left in the last position is harmless, key_count no longer covers it.
          for(std::uint32_t i = delete_pos; i < node->key_count - 1; ++i)
          {
               node->keys[i] = node->keys[i + 1];
               node->values[i] = node->values[i + 1];
          }

          node->key_count--;

          // Still big enough, nothing to rebalance. Also the way out for a root leaf,
          // since path is empty and the loop below would not run anyway.
          if(node->key_count >= BTreeNode::MIN_KEYS)
          {
               _bpm->unpin_page(current_page_id, true);
               return true;
          }

          // Underflow. Walk back up as far as the damage reaches.
          while(!path.empty())
          {
               int parent_page_id = path.back().first;
               int child_index = path.back().second;

               BufferPool::Frame* parent_frame = _bpm->fetch_page(parent_page_id);
               BTreeNode* parent = reinterpret_cast<BTreeNode*>(parent_frame->data);

               // An edge child only has one sibling, so -1 stands for "not there" and
               // every use of these is guarded below.
               int left_sibling_idx = (child_index > 0) ? child_index - 1: -1;
               int left_page_id = (left_sibling_idx != -1) ? parent->children[left_sibling_idx] : -1;

               int right_sibling_idx = (child_index < (int)parent->key_count) ? child_index + 1 : -1;
               int right_page_id = (right_sibling_idx != -1) ? parent->children[right_sibling_idx] : -1;

               BufferPool::Frame* left_sibling_frame = left_page_id != -1 ? _bpm->fetch_page(left_page_id) : nullptr;
               BufferPool::Frame* right_sibling_frame = right_page_id != -1 ? _bpm->fetch_page(right_page_id) : nullptr;

               BTreeNode* left_sibling = left_sibling_frame != nullptr ? reinterpret_cast<BTreeNode*>(left_sibling_frame->data) : nullptr;
               BTreeNode* right_sibling = right_sibling_frame != nullptr ? reinterpret_cast<BTreeNode*>(right_sibling_frame->data) : nullptr;
          
               bool is_leaf = node->is_leaf;

               // Strictly greater, not >=. A sibling sitting exactly on MIN_KEYS cannot
               // give anything away without going under itself.
               if(left_sibling != nullptr && left_sibling->key_count > BTreeNode::MIN_KEYS)
               {
                    if(is_leaf)
                    {
                         // Leaf borrow: the sibling's largest key moves straight over
                         // and the parent's separator is refreshed to match the new
                         // first key here. Data only ever lives in leaves, so nothing
                         // has to pass through the parent.
                         int borrowed_key = left_sibling->keys[left_sibling->key_count - 1];
                         int borrowed_val = left_sibling->values[left_sibling->key_count - 1];

                         for(std::uint32_t i = node->key_count; i > 0; --i)
                         {
                              node->keys[i] = node->keys[i - 1];
                              node->values[i] = node->values[i - 1];
                         }

                         node->keys[0] = borrowed_key;
                         node->values[0] = borrowed_val;

                         left_sibling->key_count--;
                         node->key_count++;

                         parent->keys[child_index - 1] = node->keys[0];
                    }
                    else
                    {
                         // Internal borrow is a rotation through the parent, not a
                         // straight move. The separator comes down into this node and
                         // the sibling's last key goes up to take its place, because
                         // the sibling's key is the one that now separates the two.
                         //shift node's keys / children right by one
                         for(std::uint32_t i = node->key_count; i > 0; --i)
                         {
                              node->keys[i] = node->keys[i - 1];
                         }
                         for(std::uint32_t i = node->key_count + 1; i > 0; --i)
                         {
                              node->children[i] = node->children[i - 1];
                         }

                         node->keys[0] = parent->keys[child_index - 1];
                         node->children[0] = left_sibling->children[left_sibling->key_count];

                         //left sibling's last key moves up to parent
                         parent->keys[child_index - 1] = left_sibling->keys[left_sibling->key_count - 1];

                         left_sibling->key_count--;
                         node->key_count++;
                    }

                    // Three pages changed, and the right sibling was fetched further up
                    // without ever being touched, so it goes back clean. Borrowing
                    // settles the underflow here, no need to look at the parent again.
                    _bpm->unpin_page(current_page_id, true);
                    _bpm->unpin_page(left_page_id, true);

                    if(right_page_id != -1) _bpm->unpin_page(right_page_id, false);

                    _bpm->unpin_page(parent_page_id, true);

                    return true;
               }

               // Same trade in the other direction: the sibling's smallest key, and
               // everything shifts left over there instead of right over here.
               if(right_sibling != nullptr && right_sibling->key_count > BTreeNode::MIN_KEYS)
               {
                    if(is_leaf)
                    {
                         int borrowed_key = right_sibling->keys[0];
                         int borrowed_val = right_sibling->values[0];

                         for(std::uint32_t i = 0; i < right_sibling->key_count - 1; ++i)
                         {
                              right_sibling->keys[i] = right_sibling->keys[i + 1];
                              right_sibling->values[i] = right_sibling->values[i + 1];
                         }

                         node->keys[node->key_count] = borrowed_key;
                         node->values[node->key_count] = borrowed_val;

                         right_sibling->key_count--;
                         node->key_count++;
                         parent->keys[child_index] = right_sibling->keys[0];
                    }
                    else 
                    {
                         node->keys[node->key_count] = parent->keys[child_index];
                         node->children[node->key_count + 1] = right_sibling->children[0];

                         parent->keys[child_index] = right_sibling->keys[0];

                         for(std::uint32_t i = 0; i < right_sibling->key_count - 1; ++i)
                         {
                              right_sibling->keys[i] = right_sibling->keys[i + 1];
                         }
                         for(std::uint32_t i = 0; i < right_sibling->key_count; ++i)
                         {
                              right_sibling->children[i] = right_sibling->children[i + 1];
                         }

                         right_sibling->key_count--;
                         node->key_count++;
                    }

                    _bpm->unpin_page(current_page_id, true);
                    _bpm->unpin_page(right_page_id, true);

                    if(left_page_id != -1) _bpm->unpin_page(left_page_id, false);

                    _bpm->unpin_page(parent_page_id, true);
                    return true;
               }

               // Nobody could spare a key, so two nodes become one. Everything here
               // pours into the left sibling and this node stops being reachable, which
               // also means its page is simply abandoned, nothing reclaims it.
               //merge with left
               if(left_page_id != -1 && left_sibling != nullptr)
               {
                    if(is_leaf)
                    {
                         for(std::uint32_t i = 0; i < node->key_count; ++i)
                         {
                              left_sibling->keys[left_sibling->key_count + i] = node->keys[i];
                              left_sibling->values[left_sibling->key_count + i] = node->values[i];
                         }

                         left_sibling->key_count += node->key_count;

                         // Pull the disappearing leaf out of the chain, or a range scan
                         // walks into a page that is no longer part of the tree.
                         left_sibling->next_leaf_page_id = node->next_leaf_page_id;
                    }
                    else
                    {
                         // Merging internal nodes needs the separator from the parent
                         // in the middle. Without it the two key ranges would sit next
                         // to each other with the boundary between them missing.
                         left_sibling->keys[left_sibling->key_count] = parent->keys[child_index - 1];
                         std::uint32_t offset = left_sibling->key_count + 1;

                         for(std::uint32_t i = 0; i < node->key_count; ++i)
                         {
                              left_sibling->keys[offset + i] = node->keys[i];
                         }
                         for(std::uint32_t i = 0; i < node->key_count + 1; ++i)
                         {
                              left_sibling->children[offset + i] = node->children[i];
                         }

                         left_sibling->key_count += node->key_count + 1;
                    }

                    // The parent loses the separator and the pointer to the node that
                    // just got absorbed. Keys and children shift by different amounts
                    // because the child being dropped sits to the right of the key.
                    for(std::uint32_t i = child_index - 1; i < parent->key_count - 1; ++i)
                    {
                         parent->keys[i] = parent->keys[i + 1];
                         parent->children[i + 1] = parent->children[i + 2];
                    }
                    parent->key_count--;

                    _bpm->unpin_page(current_page_id, true);
                    _bpm->unpin_page(left_page_id, true);

                    if(right_page_id != -1) _bpm->unpin_page(right_page_id, false);

                    // An empty root has one child left, so it is dead weight. Making
                    // that child the root is the only thing that makes the tree
                    // shorter. MIN_KEYS deliberately does not apply to a root.
                    if(parent_page_id == _root_page_id && parent->key_count == 0)
                    {
                         _root_page_id = left_page_id;
                         _bpm->unpin_page(parent_page_id, true);
                         return true;
                    }
                    if(parent->key_count >= BTreeNode::MIN_KEYS)
                    {
                         _bpm->unpin_page(parent_page_id, true);
                         return true;
                    }

                    // The parent went under too, so it becomes the node with the
                    // problem and the same three options get tried one level up.
                    //
                    // Careful: parent stays pinned on this path on purpose, it is the
                    // node being worked on now, and the next round unpins it.
                    current_page_id = parent_page_id;
                    node = parent;
                    path.pop_back();
                    continue;
               }

               // Leftmost child, so there was no left sibling to merge into. Mirror of
               // the block above, except this node survives and swallows the right one.
               //merge with right
               if(right_page_id != -1 && right_sibling != nullptr)
               {
                    if(is_leaf)
                    {
                         for(std::uint32_t i = 0; i < right_sibling->key_count; ++i)
                         {
                              node->keys[node->key_count + i] = right_sibling->keys[i];
                              node->values[node->key_count + i] = right_sibling->values[i];
                         }

                         node->key_count += right_sibling->key_count;
                         node->next_leaf_page_id = right_sibling->next_leaf_page_id;
                    }
                    else
                    {
                         node->keys[node->key_count] = parent->keys[child_index];
                         std::uint32_t offset = node->key_count + 1;

                         for(std::uint32_t i = 0; i < right_sibling->key_count; ++i)
                         {
                              node->keys[offset + i] = right_sibling->keys[i];
                         }
                         for(std::uint32_t i = 0; i < right_sibling->key_count + 1; ++i)
                         {
                              node->children[offset + i] = right_sibling->children[i];
                         }

                         node->key_count += right_sibling->key_count + 1;
                    }

                    for(std::uint32_t i = child_index; i < parent->key_count - 1; ++i)
                    {
                         parent->keys[i] = parent->keys[i + 1];
                         parent->children[i + 1] = parent->children[i + 2];
                    }
                    parent->key_count--;

                    _bpm->unpin_page(current_page_id, true);
                    _bpm->unpin_page(right_page_id, true);

                    if(left_page_id != -1) _bpm->unpin_page(left_page_id, false);

                    // Root emptied out again, and this time the surviving child is the
                    // node we merged into, so that one becomes the new root.
                    if(parent_page_id == _root_page_id && parent->key_count == 0)
                    {
                         _root_page_id = current_page_id;
                         _bpm->unpin_page(parent_page_id, true);
                         return true;
                    }
                    if(parent->key_count >= BTreeNode::MIN_KEYS)
                    {
                         _bpm->unpin_page(parent_page_id, true);
                         return true;
                    }
                    
                    current_page_id = parent_page_id;
                    node = parent;
                    path.pop_back();
                    continue;
               }

               // A node with a parent always has at least one sibling, so getting here
               // means the tree is already malformed. Bail out rather than loop.
               //should not reach here
               _bpm->unpin_page(parent_page_id, true);
               break;
          }

          // Out of the loop with the page still pinned: either the merging reached the
          // root, or the malformed-tree break above fired. Either way this is the last
          // node that was touched, so release it dirty.
          _bpm->unpin_page(current_page_id, true);
          return true;
     }

     // Overwrites the value of an existing key. No inserting, no rebalancing: key_count
     // does not move, so the shape of the tree cannot change here.
     bool BTreeIndex::update(int key, int value)
     {
          int n;
          if(!search(key, &n)) return false;

          int current_page_id = _root_page_id;
          
          BufferPool::Frame* frame = _bpm->fetch_page(current_page_id);
          BTreeNode* node = reinterpret_cast<BTreeNode*>(frame->data);
          
          while(!node->is_leaf)
          {
               int child_index = node->key_count;

               for(std::uint32_t i = 0; i < node->key_count; ++i)
               {
                    if(key < node->keys[i])
                    {
                         child_index = i;
                         break;
                    }
               }

               int next_page_id = node->children[child_index];
               _bpm->unpin_page(current_page_id, false);

               current_page_id = next_page_id;
               frame = _bpm->fetch_page(current_page_id);
               node = reinterpret_cast<BTreeNode*>(frame->data);
          }

          for(std::uint32_t i = 0; i < node->key_count; ++i)
          {
               if(node->keys[i] == key)
               {
                    node->values[i] = value;
                    _bpm->unpin_page(current_page_id, true);
                    return true;
               }
          }

          //not found
          _bpm->unpin_page(current_page_id, false);
          return false;
     }
}
