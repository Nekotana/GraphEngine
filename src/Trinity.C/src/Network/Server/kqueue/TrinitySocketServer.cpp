// Graph Engine
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
#include <os/os.h>
#if ((!defined(__linux__)) && (defined(__unix__) || (defined(__APPLE__) && defined(__MACH__))))
#include "TrinitySocketServer.h"

namespace Trinity
{
    namespace Network
    {
        int sock_fd;
        int kqueue_fd;
        std::thread socket_accept_thread;

        void AwaitRequest(void* &_pContext)
        {
            struct kevent kevt_out;
            while (true)
            {
                if (1 == kevent(kqueue_fd, NULL, 0, &kevt_out, 1, NULL))
                {
                    if ((kevt_out.flags & EV_EOF) || (kevt_out.flags & EV_ERROR))
                    {
                        struct kevent kevt;
                        EV_SET(&kevt, kevt_out.ident, EVFILT_READ, EV_DELETE, 0, 0, NULL);
                        if (-1 == kevent(kqueue_fd, &kevt, 1, NULL, 0, NULL))
                        {
                            fprintf(stderr, "cannot delete fd from kqueue");
                        }
                        close(kevt_out.ident);
                    }
                    else if (kevt_out.flags & EVFILT_READ)
                    {
                        PerSocketContextObjectSlim* pContext = GetPerSocketContextObject(kevt_out.ident);
                        _pContext = pContext;
                        //TODO handshake
                        ProcessRecv(pContext);
                        break;
                    }
                }
            }
        }

        void SocketAcceptThreadProc()
        {
            fprintf(stderr, "socket accept thread ...\n");
            while (true)
            {
                int connected_sock_fd = AcceptConnection(sock_fd);
                if (-1 == connected_sock_fd)
                    continue;
                PerSocketContextObjectSlim * pContext = AllocatePerSocketContextObjectSlim(connected_sock_fd);
                AddPerSocketContextObject(pContext);
                struct kevent kevt;
                EV_SET(&kevt, connected_sock_fd, EVFILT_READ, EV_ADD | EV_ENABLE | EV_DISPATCH, 0, 0, NULL);
                //TODO handshake
                if (-1 == kevent(kqueue_fd, &kevt, 1, NULL, 0, NULL))
                {
                    fprintf(stderr, "cannot register fd to kqueue\n");
                    RemovePerSocketContextObject(connected_sock_fd);
                    FreePerSocketContextObjectSlim(pContext);
                    close(connected_sock_fd);
                }
            }
        }

        int ShutdownSocketServer()
        {
            socket_accept_thread.join();
            close(sock_fd);
            close(kqueue_fd);
            return 0;
        }

        bool RearmFD(int fd)
        {
            struct kevent kevt;
            EV_SET(&kevt, fd, EVFILT_READ, EV_ENABLE, 0, 0, NULL);
            return -1 != kevent(kqueue_fd, &kevt, 1, NULL, 0, NULL);
        }

        //bool ProcessAccept()
        //{
        //    fprintf(stderr, "process accept ...\n");
        //    sockaddr addr;
        //    socklen_t addrlen = sizeof(sockaddr);
        //    int connected_sock_fd = accept4(sock_fd, &addr, &addrlen, SOCK_NONBLOCK);
        //    if (-1 == connected_sock_fd)
        //    {
        //        return false;
        //    }
        //    struct kevent kevt;
        //    EV_SET(&kevt, connected_sock_fd, EVFILT_READ, EV_ADD | EV_ENABLE | EV_DISPATCH, 0, 0, NULL);
        //    if (-1 == kevent(kqueue_fd, &kevt, 1, NULL, 0, NULL))
        //        return false;
        //    return true;
        //}

        int InitializeEventMonitor(int sockfd)
        {
            sock_fd = sockfd;
            kqueue_fd = kqueue();
            if (-1 == kqueue_fd)
            {
                return false;
            }
            fprintf(stderr, "kqueue: %d\n", kqueue_fd);
            socket_accept_thread = std::thread(SocketAcceptThreadProc);
            return true;
        }
    }
}
#endif//platform