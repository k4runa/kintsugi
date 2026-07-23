#include "../include/btree_node.h"
#include "../include/buffer_pool.h"
#include <algorithm>
#include <cstdint>
#include <ios>
#include <utility>
#include <vector>


namespace Kintsugi::Tree
{
     BTreeIndex::BTreeIndex(Kintsugi::BufferPool::BufferPoolManager* bpm) : _bpm(bpm) 
     {
          BufferPool::Frame* frame = _bpm->new_page(&_root_page_id);
          BTreeNode*         node  = reinterpret_cast<BTreeNode*>(frame->data);

          node->is_leaf           = true;
          node->key_count         = 0;
          node->next_leaf_page_id = -1;

          _bpm->unpin_page(_root_page_id, true);
     }

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
               }
          }
     }

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

          for(std::uint32_t i = node->key_count; i > insert_pos; --i)
          {
               node->keys[i]   = node->keys[i - 1];
               node->values[i] = node->values[i - 1];
          }

          node->keys[insert_pos]   = key;
          node->values[insert_pos] = value;

          node->key_count++;
     }

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

          for(std::uint32_t i = node->key_count + 1; i >insert_pos + 1; --i)
          {
               node->children[i] = node->children[i - 1];
          }

          node->children[insert_pos + 1] = right_page_id;
          node->key_count++;
     }

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
          right->next_leaf_page_id  = left->next_leaf_page_id;
          left->next_leaf_page_id   = right_page_id;
          right->is_leaf            = true;

          *out_middle_key = right->keys[0];

          _bpm->unpin_page(right_page_id, true);

          return right_page_id;
     }

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

          int right_child_count = 0;
          for(std::uint32_t i = middle_index + 1; i <= left->key_count; ++i)
          {
               right->children[right_child_count++] = left->children[i];
          }

          left->key_count = middle_index;

          _bpm->unpin_page(right_page_id, true);
          return right_page_id;
     }

     bool BTreeIndex::insert(int key, int value)
     {
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
          if(node->key_count <= BTreeNode::MAX_KEYS)
          {
               _bpm->unpin_page(current_page_id, true);
               return true;
          }

          int middle_key;
          int right_page_id = split_leaf(current_page_id, node, &middle_key);
          _bpm->unpin_page(current_page_id, true);

          int left_page_id = current_page_id;

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

     bool BTreeIndex::delete_k(int key)
     {
          std::vector<std::pair<int,int>> path;
          
          int current_page_id = _root_page_id;
          BufferPool::Frame* frame = _bpm->fetch_page(current_page_id);
          BTreeNode* node  = reinterpret_cast<BTreeNode*>(frame->data); 

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

          // not found
          if(delete_pos == node->key_count)
          {
               _bpm->unpin_page(current_page_id, false);
               return false;
          }
          
          for(std::uint32_t i = delete_pos; i < node->key_count - 1; ++i)
          {
               node->keys[i] = node->keys[i + 1];
               node->values[i] = node->values[i + 1];
          }

          node->key_count--; 
          if(node->key_count >= BTreeNode::MIN_KEYS)
          {
               _bpm->unpin_page(current_page_id, true);
               return true;
          }
          while(!path.empty())
          {
               int parent_id = path.back().first;
               int child_index = path.back().second;

               BufferPool::Frame* parent_frame = _bpm->fetch_page(parent_id);
               BTreeNode* parent = reinterpret_cast<BTreeNode*>(parent_frame->data);

               int left_sibling_idx = (child_index > 0) ? child_index - 1 : -1;
               int left_page_id = (left_sibling_idx != -1) ? parent->children[left_sibling_idx] : -1;

               int right_sibling_idx = (child_index < parent->key_count) ? child_index + 1 : -1;
               int right_page_id = (right_sibling_idx != -1 ) ? parent->children[right_sibling_idx] : -1;

               BufferPool::Frame* left_sibling_frame = left_page_id != -1 ? _bpm->fetch_page(left_page_id) : nullptr;
               BufferPool::Frame* right_sibling_frame = right_page_id != -1 ? _bpm->fetch_page(right_page_id) : nullptr;

               BTreeNode* left_sibling = left_sibling_frame != nullptr ? reinterpret_cast<BTreeNode*>(left_sibling_frame->data) : nullptr;
               BTreeNode* right_sibling = right_sibling_frame != nullptr ? reinterpret_cast<BTreeNode*>(right_sibling_frame->data) : nullptr;

               if(left_sibling != nullptr && left_sibling->key_count > BTreeNode::MIN_KEYS)
               {
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

                    _bpm->unpin_page(current_page_id, true);
                    _bpm->unpin_page(left_page_id, true);
                    if(right_page_id != -1) _bpm->unpin_page(right_page_id, false);
                    _bpm->unpin_page(parent_id, true);

                    return true;
                    
               }
               else 
               {
                    if(right_sibling != nullptr && right_sibling->key_count > BTreeNode::MIN_KEYS)
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

                         _bpm->unpin_page(current_page_id, true);
                         _bpm->unpin_page(right_page_id, true);
                         if(left_page_id != -1) _bpm->unpin_page(left_page_id, false);
                         _bpm->unpin_page(parent_id, true); 

                         return true;
                    }
               }

               // borrow failed, try merge
               if(left_page_id != -1 && left_sibling != nullptr) // try left first 
               {
                    for(std::uint32_t i = 0; i < node->key_count; ++i)
                    {
                         left_sibling->keys[left_sibling->key_count + i] = node->keys[i];
                         left_sibling->values[left_sibling->key_count + i] = node->values[i];
                    }

                    left_sibling->key_count += node->key_count;
                    left_sibling->next_leaf_page_id = node->next_leaf_page_id;

                    for(std::uint32_t i = child_index - 1; i < parent->key_count - 1; ++i)
                    {
                         parent->keys[i] = parent->keys[i + 1];
                         parent->children[i + 1] = parent->children[i + 2];
                    }

                    parent->key_count--;

                    _bpm->unpin_page(current_page_id, true);
                    _bpm->unpin_page(left_page_id, true);

                    if(right_page_id != -1) _bpm->unpin_page(right_page_id, false);

                    if(parent_id == _root_page_id && parent->key_count == 0)
                    {
                         _root_page_id = left_page_id;
                         _bpm->unpin_page(parent_id, true);
                         return true;
                    }

                    if(parent->key_count >= BTreeNode::MIN_KEYS)
                    {
                         _bpm->unpin_page(parent_id, true);
                         return true;
                    }

                    current_page_id = parent_id;
                    node = parent;
                    path.pop_back();
               }
               else //if left fail, try with right
               {
                    if(right_page_id != -1 && right_sibling != nullptr)
                    {
                         for(std::uint32_t i = 0; i < right_sibling->key_count; ++i)
                         {
                              node->keys[node->key_count + i] = right_sibling->keys[i];
                              node->values[node->key_count + i] = right_sibling->values[i];
                         }

                         node->key_count += right_sibling->key_count;
                         node->next_leaf_page_id = right_sibling->next_leaf_page_id;

                         for(std::uint32_t i = child_index; i < parent->key_count - 1; ++i)
                         {
                              parent->keys[i] = parent->keys[i + 1];
                              parent->children[i + 1] = parent->children[i + 2]; 
                         }

                         parent->key_count--;

                         _bpm->unpin_page(current_page_id, true);
                         _bpm->unpin_page(right_page_id, true);
                         if(left_page_id != -1) _bpm->unpin_page(left_page_id, false);

                         if(parent_id == _root_page_id && parent->key_count == 0)
                         {
                              _root_page_id = right_page_id;
                              _bpm->unpin_page(parent_id, true);
                              return true;
                         }

                         if(parent->key_count >= BTreeNode::MIN_KEYS)
                         {
                              _bpm->unpin_page(parent_id, true);
                              return true;
                         }

                         current_page_id = parent_id;
                         node = parent;
                         path.pop_back();
                    }
               }
          }

          _bpm->unpin_page(current_page_id, true);
          
          return true;
     }
}
