/*
 * Copyright (C) 2019  Hariharan Devarajan, Keith Bateman
 *
 * This file is part of Basket
 * 
 * Basket is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program.  If not, see
 * <http://www.gnu.org/licenses/>.
 */

#ifndef INCLUDE_BASKET_QUEUE_QUEUE_H_
#define INCLUDE_BASKET_QUEUE_QUEUE_H_

/**
 * Include Headers
 */
#include <basket/communication/rpc_lib.h>
#include <basket/common/singleton.h>
#include <basket/common/debug.h>
/** MPI Headers**/
#include <mpi.h>
/** RPC Lib Headers**/
#include <rpc/server.h>
#include <rpc/client.h>
/** Boost Headers **/
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/containers/deque.hpp>
#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/interprocess/sync/interprocess_mutex.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <boost/algorithm/string.hpp>
/** Standard C++ Headers**/
#include <iostream>
#include <functional>
#include <utility>
#include <memory>
#include <string>

/** Namespaces Uses **/
namespace bip = boost::interprocess;


namespace basket {
/**
 * This is a Distributed Queue Class. It uses shared memory +
 * RPC + MPI to achieve the data structure.
 *
 * @tparam MappedType, the value of the Queue
 */
template<typename MappedType>
class queue {
 private:
  /** Class Typedefs for ease of use **/
  typedef bip::allocator<MappedType,
                         bip::managed_shared_memory::segment_manager>
  ShmemAllocator;
  typedef boost::interprocess::deque<MappedType, ShmemAllocator> Queue;

  /** Class attributes**/
  int comm_size, my_rank, num_servers;
  uint16_t  my_server;
  std::shared_ptr<RPC> rpc;
  really_long memory_allocated;
  bool is_server;
  boost::interprocess::managed_shared_memory segment;
  std::string name, func_prefix;
  Queue *my_queue;
  boost::interprocess::interprocess_mutex* mutex;

 public:
  ~queue();

  explicit queue(std::string name_,
                 bool is_server_,
                 uint16_t my_server_,
                 int num_servers_);
  bool Push(MappedType data, uint16_t key_int);
  std::pair<bool, MappedType> Pop(uint16_t key_int);
  bool WaitForElement(uint16_t key_int);
  size_t Size(uint16_t key_int);
};

#include "queue.cpp"

}  // namespace basket

#endif  // INCLUDE_BASKET_QUEUE_QUEUE_H_