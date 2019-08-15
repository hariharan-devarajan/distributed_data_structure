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

#include <basket/communication/rpc_lib.h>

RPC::~RPC() {
    if (!shared_init) {
        delete server_list.single;
    }
    else if (is_server) {
        bip::shared_memory_object::remove(name.c_str());
    }

    if (is_server) {
        switch (BASKET_CONF->RPC_IMPLEMENTATION) {
#ifdef BASKET_ENABLE_RPCLIB
            case RPCLIB: {
          // Twiddle thumbs
          break;
        }
#endif
#ifdef BASKET_ENABLE_THALLIUM_TCP
            case THALLIUM_TCP:
#endif
#ifdef BASKET_ENABLE_THALLIUM_ROCE
                case THALLIUM_ROCE:
#endif
#if defined(BASKET_ENABLE_THALLIUM_TCP) || defined(BASKET_ENABLE_THALLIUM_ROCE)
            {
                thallium_engine->finalize();
                break;
            }
#endif
        }

    }
}

RPC::RPC() : isInitialized(false), shared_init(false),
             is_server(BASKET_CONF->IS_SERVER),
             my_server(BASKET_CONF->MY_SERVER),
             num_servers(BASKET_CONF->NUM_SERVERS),
             server_on_node(BASKET_CONF->SERVER_ON_NODE),
             server_port(RPC_PORT) {
    AutoTrace trace = AutoTrace("RPC");
    if (!isInitialized) {
        int len;
        char proc_name[MPI_MAX_PROCESSOR_NAME];
        MPI_Get_processor_name(proc_name, &len);
        processor_name = std::string(proc_name);  // so we can compare to servers

        server_list.single = new std::vector<std::string>();
        fstream file;
        file.open(BASKET_CONF->SERVER_LIST,ios::in);
        if (file.is_open()) {
            std::string file_line;
            std::string server_node;
            std::string server_network;
            server_on_node = false;  // in case there is no server on node
            while (getline(file, file_line)) {
                int split_loc = file_line.find(':');  // split to node and net
                if (split_loc != std::string::npos) {
                    server_node = file_line.substr(0, split_loc);
                    server_network = file_line.substr(split_loc+1, std::string::npos);
                } else {
                    // no special network
                    server_node = file_line;
                    server_network = file_line;
                }
                // server list is list of network interfaces
                server_list.single->push_back(std::string(server_network));
            }
        } else {
            printf("Error: Can't open server list file %s\n", BASKET_CONF->SERVER_LIST.c_str());
            exit(EXIT_FAILURE);
        }
        file.close();

        /* Initialize MPI rank and size of world */
        MPI_Comm_size(MPI_COMM_WORLD, &comm_size);
        MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);

        /* if current rank is a server */
        if (is_server) {
            switch (BASKET_CONF->RPC_IMPLEMENTATION) {
#ifdef BASKET_ENABLE_RPCLIB
                case RPCLIB: {
                  rpclib_server = std::make_shared<rpc::server>(server_port+my_server);
                  break;
                }
#endif
#ifdef BASKET_ENABLE_THALLIUM_TCP
                case THALLIUM_TCP: {
                   engine_init_str = BASKET_CONF->TCP_CONF + "://" +
                                                  std::string(processor_name) +
                                                  ":" +
                                                  std::to_string(server_port + my_server);
                    break;
                }
#endif
#ifdef BASKET_ENABLE_THALLIUM_ROCE
                case THALLIUM_ROCE: {
                    engine_init_str = BASKET_CONF->VERBS_CONF + "://" +
                            BASKET_CONF->VERBS_DOMAIN + "://" +
                            std::string(processor_name) +
                            ":" +
                            std::to_string(server_port+my_server);
                    break;
                }
#endif
            }
        } else {
            switch (BASKET_CONF->RPC_IMPLEMENTATION) {
#ifdef BASKET_ENABLE_RPCLIB
                case RPCLIB: {
                  break;
                }
#endif
#ifdef BASKET_ENABLE_THALLIUM_TCP
                case THALLIUM_TCP: {
                    thallium_engine = Singleton<tl::engine>::GetInstance(BASKET_CONF->TCP_CONF,
                                                                         MARGO_CLIENT_MODE);
                    break;
                }
#endif
#ifdef BASKET_ENABLE_THALLIUM_ROCE
                case THALLIUM_ROCE: {
                  thallium_engine = Singleton<tl::engine>::GetInstance(BASKET_CONF->VERBS_CONF,
                                           MARGO_CLIENT_MODE);
                  break;
                }
#endif
            }
        }

        /* Create server list from the broadcast list*/
        isInitialized = true;

        MPI_Barrier(MPI_COMM_WORLD);
        run();
        MPI_Barrier(MPI_COMM_WORLD);
    }
}

// RPC::RPC(std::string server_list_, bool is_server_) : isInitialized(false), shared_init(false), is_server(is_server_), server_port(RPC_PORT) {
//     AutoTrace trace = AutoTrace("RPC", server_list_, is_server_);
//     if (!isInitialized) {
//         int len;
//         char proc_name[MPI_MAX_PROCESSOR_NAME];
//         MPI_Get_processor_name(proc_name, &len);
//         processor_name = std::string(proc_name);  // so we can compare to servers

//         server_list.single = new std::vector<std::string>();
//         fstream file;
//         file.open(server_list_,ios::in);
//         if (file.is_open()) {
//             std::string file_line;
//             std::string server_node;
//             std::string server_network;

//             server_on_node = false;  // in case there is no server on node
//             while (getline(file, file_line)) {
//                 int split_loc = file_line.find(':');  // split to node and net
//                 if (split_loc != std::string::npos) {
//                     server_node = file_line.substr(0, split_loc);
//                     server_network = file_line.substr(split_loc+1, std::string::npos);
//                 } else {
//                     // no special network
//                     server_node = file_line;
//                     server_network = file_line;
//                 }
//                 if (server_node.compare(processor_name) == 0) {
//                     my_server = server_list.single->size();  // this server is my server
//                     server_on_node = true;  // there is a server on node
//                 }
//                 // server list is list of network interfaces
//                 server_list.single->push_back(std::string(server_network));
//             }
//         } else {
//             printf("Error: Can't open server list file %s\n", server_list_);
//             exit(EXIT_FAILURE);
//         }
//         file.close();
//         num_servers = server_list.single->size();

//         /* Initialize MPI rank and size of world */
//         MPI_Comm_size(MPI_COMM_WORLD, &comm_size);
//         MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);

//         /* if current rank is a server */
//         if (is_server) {
//             switch (BASKET_CONF->RPC_IMPLEMENTATION) {
// #ifdef BASKET_ENABLE_RPCLIB
//                 case RPCLIB: {
//                   rpclib_server = std::make_shared<rpc::server>(server_port+my_server);
//                   break;
//                 }
// #endif
// #ifdef BASKET_ENABLE_THALLIUM_TCP
//                 case THALLIUM_TCP: {
//                    engine_init_str = BASKET_CONF->TCP_CONF + "://" +
//                                                   std::string(processor_name) +
//                                                   ":" +
//                                                   std::to_string(server_port + my_server);
//                     break;
//                 }
// #endif
// #ifdef BASKET_ENABLE_THALLIUM_ROCE
//                 case THALLIUM_ROCE: {
//                     engine_init_str = BASKET_CONF->VERBS_CONF + "://" +
//                             BASKET_CONF->VERBS_DOMAIN + "://" +
//                             std::string(processor_name) +
//                             ":" +
//                             std::to_string(server_port+my_server);
//                     break;
//                 }
// #endif
//             }
//         } else {
//             switch (BASKET_CONF->RPC_IMPLEMENTATION) {
// #ifdef BASKET_ENABLE_RPCLIB
//                 case RPCLIB: {
//                   break;
//                 }
// #endif
// #ifdef BASKET_ENABLE_THALLIUM_TCP
//                 case THALLIUM_TCP: {
//                     thallium_engine = Singleton<tl::engine>::GetInstance(BASKET_CONF->TCP_CONF,
//                                                                          MARGO_CLIENT_MODE);
//                     break;
//                 }
// #endif
// #ifdef BASKET_ENABLE_THALLIUM_ROCE
//                 case THALLIUM_ROCE: {
//                   thallium_engine = Singleton<tl::engine>::GetInstance(BASKET_CONF->VERBS_CONF,
//                                            MARGO_CLIENT_MODE);
//                   break;
//                 }
// #endif
//             }
//         }

//         /* Create server list from the broadcast list*/
//         isInitialized = true;

//         MPI_Barrier(MPI_COMM_WORLD);
//         run();
//         MPI_Barrier(MPI_COMM_WORLD);
//     }
// }

RPC::RPC(std::string name_, bool is_server_, uint16_t my_server_,
         int num_servers_, bool server_on_node_, std::string processor_name_) :
        isInitialized(false), my_server(my_server_), is_server(is_server_),
        server_port(RPC_PORT),
        server_on_node(server_on_node_),
        processor_name(processor_name_),
        num_servers(num_servers_), name(name_),
        memory_allocated(1024ULL * 1024ULL), segment(),
        shared_init(true) {
    AutoTrace trace = AutoTrace("RPC", name_, is_server_, my_server_,
                                num_servers_);
    if (!isInitialized) {
        int total_len;
        char *final_server_list;
        /* Initialize MPI rank and size of world */
        MPI_Comm_size(MPI_COMM_WORLD, &comm_size);
        MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);

        /* Create a server communication group */
        MPI_Comm scomm;
        MPI_Comm_split(MPI_COMM_WORLD, is_server, my_rank, &scomm);
        name = name + "_" + std::to_string(my_server);
        /* if current rank is a server */
        if (is_server) {
            /* Get hostname where server is running Name */
            int len;
            char processor_name[MPI_MAX_PROCESSOR_NAME];
            if (processor_name_.empty()) {
                MPI_Get_processor_name(processor_name, &len);
            } else {
                len = processor_name_.length();
                strncpy(processor_name, processor_name_.c_str(), len + 1);
            }

            /* Get current servers rank in the server group starts with 1*/
            int server_rank;
            MPI_Comm_rank(scomm, &server_rank);
            server_rank += 1;
            // int ranks_per_server = comm_size / num_servers;
            // int server_rank = (my_rank / ranks_per_server) + 1;

            /* Synchronize hostnames accross all servers*/
            int *recvcounts = NULL;
            if (server_rank == 1)
                recvcounts = static_cast<int *>(
                        malloc(num_servers * sizeof(int)));
            MPI_Gather(&len, 1, MPI_INT, recvcounts, 1, MPI_INT, 0, scomm);
            total_len = 0;
            int *displs = NULL;
            char *totalstring = NULL;
            /* if it is the first server*/
            if (server_rank == 1) {
                displs = static_cast<int *>(malloc(num_servers * sizeof(int)));
                displs[0] = 0;
                total_len += recvcounts[0] + 1;
                for (int i = 1; i < num_servers; i++) {
                    total_len += recvcounts[i] + 1;
                    /* Line above: plus one for space or \0 after words */
                    displs[i] = displs[i - 1] + recvcounts[i - 1] + 1;
                }
                /* allocate string, pre-fill with spaces and null terminator */
                totalstring = static_cast<char *>(malloc(total_len * sizeof(char)));
                for (int i = 0; i < total_len - 1; i++)
                    totalstring[i] = ',';
                totalstring[total_len - 1] = '\0';
            }
            MPI_Gatherv(processor_name, len, MPI_CHAR, totalstring, recvcounts,
                        displs, MPI_CHAR, 0, scomm);
            /* We get all the server names for RPC call*/
            if (server_rank == 1) {
                /* Broadcast server_names to all processors*/
                MPI_Bcast(&total_len, 1, MPI_INT, 0, scomm);
                final_server_list = static_cast<char *>(
                        malloc(total_len * sizeof(char)));
                snprintf(final_server_list, total_len, "%s", totalstring);
                MPI_Bcast(totalstring, total_len, MPI_CHAR, 0, scomm);
                /* free data structures*/
                free(totalstring);
                free(displs);
                free(recvcounts);
            } else {
                /* Broadcast server_names to all processors*/
                MPI_Bcast(&total_len, 1, MPI_INT, 0, scomm);
                final_server_list = static_cast<char *>(
                        malloc(total_len * sizeof(char)));
                MPI_Bcast(final_server_list, total_len, MPI_CHAR, 0, scomm);
            }
            switch (BASKET_CONF->RPC_IMPLEMENTATION) {
#ifdef BASKET_ENABLE_RPCLIB
                case RPCLIB: {
                  rpclib_server = std::make_shared<rpc::server>(server_port+my_server_);
                  break;
                }
#endif
#ifdef BASKET_ENABLE_THALLIUM_TCP
                case THALLIUM_TCP: {
                   engine_init_str = BASKET_CONF->TCP_CONF + "://" +
                                                  std::string(processor_name) +
                                                  ":" +
                                                  std::to_string(server_port + my_server_);
                    break;
                }
#endif
#ifdef BASKET_ENABLE_THALLIUM_ROCE
                case THALLIUM_ROCE: {
                    engine_init_str = BASKET_CONF->VERBS_CONF + "://" +
                            BASKET_CONF->VERBS_DOMAIN + "://" +
                            std::string(processor_name) +
                            ":" +
                            std::to_string(server_port+my_server_);
                    break;
                }
#endif
            }
            std::string final_server_list_str(final_server_list);
            std::vector<std::string> temp_list = std::vector<std::string>();
            boost::split(temp_list, final_server_list_str, boost::is_any_of(","));
            free(final_server_list);
            /* Delete existing instance of shared memory space*/
            bip::shared_memory_object::remove(name.c_str());
            /* allocate new shared memory space */
            segment = bip::managed_shared_memory(bip::create_only, name.c_str(),
                                                 memory_allocated);
            ShmemAllocator alloc_inst(segment.get_segment_manager());
            server_list.shared = segment.construct<MyVector>("MyVector")(alloc_inst);
            for (auto element : temp_list) {
                server_list.shared->push_back(CharStruct(element));
            }
        } else {
            switch (BASKET_CONF->RPC_IMPLEMENTATION) {
#ifdef BASKET_ENABLE_RPCLIB
                case RPCLIB: {
                  break;
                }
#endif
#ifdef BASKET_ENABLE_THALLIUM_TCP
                case THALLIUM_TCP: {
                    thallium_engine = Singleton<tl::engine>::GetInstance(BASKET_CONF->TCP_CONF,
                                                                         MARGO_CLIENT_MODE);
                    break;
                }
#endif
#ifdef BASKET_ENABLE_THALLIUM_ROCE
                case THALLIUM_ROCE: {
                  thallium_engine = Singleton<tl::engine>::GetInstance(BASKET_CONF->VERBS_CONF,
                                           MARGO_CLIENT_MODE);
                  break;
                }
#endif
            }
        }
        MPI_Barrier(MPI_COMM_WORLD);
        if (!is_server) {
            segment = bip::managed_shared_memory(bip::open_only, name.c_str());
            std::pair<MyVector *, bip::managed_shared_memory::size_type> res;
            res = segment.find<MyVector>("MyVector");
            server_list.shared = res.first;
        }

        /* Create server list from the broadcast list*/
        isInitialized = true;
        MPI_Barrier(MPI_COMM_WORLD);
        run();
        MPI_Barrier(MPI_COMM_WORLD);
    }
}

void RPC::run(size_t workers) {
    AutoTrace trace = AutoTrace("RPC::run", workers);
    if (is_server){
        switch (BASKET_CONF->RPC_IMPLEMENTATION) {
#ifdef BASKET_ENABLE_RPCLIB
            case RPCLIB: {
                    rpclib_server->async_run(workers);
                break;
            }
#endif
#ifdef BASKET_ENABLE_THALLIUM_TCP
            case THALLIUM_TCP:
#endif
#ifdef BASKET_ENABLE_THALLIUM_ROCE
            case THALLIUM_ROCE:
#endif
#if defined(BASKET_ENABLE_THALLIUM_TCP) || defined(BASKET_ENABLE_THALLIUM_ROCE)
                {
                    thallium_engine = Singleton<tl::engine>::GetInstance(engine_init_str, THALLIUM_SERVER_MODE,true,RPC_THREADS);
                    break;
                }
#endif
        }
    }
}
