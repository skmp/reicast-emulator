#include "build.h"

#if HOST_OS == OS_WINDOWS
#include <Winsock2.h>
#pragma comment (lib, "wsock32.lib")
#else
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#endif

#include "refrend_base.h"

#include <memory>
#include <atomic>


void error(const char *msg)
{
    perror(msg);
    exit(1);
}

enum RRI_DebugCommands {
    RRIBC_BAD = 0,
    RRIBC_OK = 1,

    RRIBC_Hello = 2,

    RRIBC_SetStep,
    RRIBC_GetBufferData,

    RRIBC_StepNotification,

    RRIBC_ClearBuffers,
    RRIBC_PeelBuffers,
    RRIBC_SummarizeStencilOr,
    RRIBC_SummarizeStencilAnd,
    RRIBC_ClearPixelsDrawn,
    RRIBC_GetPixelsDrawn,
    RRIBC_AddFpuEntry,
    RRIBC_ClearFpuEntries,
    RRIBC_GetColorOutputBuffer,
    RRIBC_RenderParamTags,
    RRIBC_RasterizeTriangle,
};

struct RefRendDebug: RefRendInterface
{
    atomic<int> waitingOK;

    atomic<bool> stepping;
    atomic<bool> sending;
    
    int sockFd = -1, clientFd = -1;

    cThread serverThread;
    cMutex socketMutex;
    
    unique_ptr<RefRendInterface> backend;

    RefRendDebug(RefRendInterface* backend) : serverThread(STATIC_FORWARD(RefRendDebug, ServerThread), this) {
        this->backend.reset(backend);
    }

    void* ServerThread() {
        static bool init = false;
        if (!init) {
#if HOST_OS == OS_WINDOWS
            static WSADATA wsaData;
            if (WSAStartup(MAKEWORD(2, 0), &wsaData) != 0)
                die("WSAStartup fail");
#endif
            init = true;
        }

        struct sockaddr_in serv_addr, cli_addr;
        sockFd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockFd < 0)
            error("ERROR opening socket");
        memset((char *)&serv_addr, 0, sizeof(serv_addr));
        int portno = 8828;
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_addr.s_addr = INADDR_ANY;
        serv_addr.sin_port = htons(portno);
        if (::bind(sockFd, (sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
            error("ERROR on binding");
        listen(sockFd, 5);

        while(sockFd != -1)
        {
            int fd = accept(sockFd, nullptr, nullptr);
            if (fd < 0) 
            {
                return nullptr;
            }

            socketMutex.Lock();
            clientFd = fd;
            waitingOK = 0;
            sending = false;
            stepping = false;
            socketMutex.Unlock();
            

            WriteCommand(RRIBC_Hello);

            while (clientFd != -1) {
                auto command = ReadCommand();

                socketMutex.Lock();

                switch (command)
                {
                case RRIBC_OK:
                    if (waitingOK <= 0) {
                        CloseSocket();
                    } else {
                        waitingOK--;
                    }
                    
                    break;

                case RRIBC_SetStep:
                    sending = ReadData<bool>();
                    stepping = ReadData<bool>();
                    WriteCommand(RRIBC_OK);
                    break;
                
                case RRIBC_GetBufferData:
                    WriteCommand(RRIBC_OK);
                    WriteBytes(backend->DebugGetAllBuffers(), 32 * 32 * 4 * 6);
                    break;

                default:
                case RRIBC_BAD:
                    CloseSocket();
                    break;
                }

                socketMutex.Unlock();
            }

            socketMutex.Lock();
            waitingOK = 0;
            sending = false;
            stepping = false;
            socketMutex.Unlock();
        }
        return 0;
    }

    void CloseSocket() {
        auto socket = clientFd;
        clientFd = -1;
        if (socket != -1) {
#if HOST_OS == OS_WINDOWS
            shutdown(socket, SD_BOTH);
            closesocket(socket);
#else
            shutdown(socket, SHUT_RD);
            close(socket);
#endif
        }
    }

    void CloseServer() {
        auto socket = sockFd;
        sockFd = -1;
        if (socket != -1)  { 
#if HOST_OS == OS_WINDOWS
            shutdown(socket, SD_BOTH);
            closesocket(socket);
#else
            shutdown(socket, SHUT_RD); 
            close(socket);
#endif
        }
    }

    bool send_all(const void* data, size_t len) {
        if (clientFd == -1)
            return false;

        auto ptr = reinterpret_cast<const char*>(data);
        auto sz = len;

        do {
            auto rv = send(clientFd, ptr, sz, 0);
            if (rv <= 0)
                return false;
            ptr += rv;
            sz -= rv;
        } while (sz > 0);

        return true;
    }

    bool recv_all(void* data, size_t len) {
        if (clientFd == -1)
            return false;

        auto ptr = reinterpret_cast<char*>(data);
        auto sz = len;

        do {
            auto rv = recv(clientFd, ptr, sz, 0);
            if (rv <= 0)
                return false;
            ptr += rv;
            sz -= rv;
        } while (sz > 0);

        return true;
    }

    void WriteCommand(RRI_DebugCommands command) {
        char cmd = command;
        send_all(&cmd, 1);
    }

    void WriteBytes(const void* bytes, int count) {
        send_all(bytes, count);
    }

    template<typename T>
    void WriteData(const T& data) {
        WriteBytes(&data, sizeof(data));
    }

    void WriteBuffers() {
        WriteBytes(backend->DebugGetAllBuffers(), 32 * 32 * 4 * 6);
    }

    template<typename T>
    T ReadData() {
        T rv;
        if (recv_all(&rv, sizeof(rv)))
            return rv;
        else
            return T();
    }

    RRI_DebugCommands ReadCommand() {
        if (clientFd != -1) {
            auto cmd = ReadData<char>();
            return (RRI_DebugCommands)cmd;
        } else {
            return RRIBC_BAD;
        }
    }
    
    void WaitForStep() {
        if (clientFd != -1 && stepping) {
            waitingOK++;
            WriteCommand(RRIBC_StepNotification);        
            while(waitingOK != 0)
                ;
        }
    }

    // backend specific init
    virtual bool Init() {
        if (!backend->Init())
            return false;

        serverThread.Start();
        
        return true;
    }

#define WCH(name) socketMutex.Lock(); if (sending) { WriteCommand(RRIBC_##name);
#define WCE() } socketMutex.Unlock()

    // Clear the buffers
    virtual void ClearBuffers(u32 paramValue, float depthValue, u32 stencilValue) {
        
        backend->ClearBuffers(paramValue, depthValue, stencilValue);

        WCH(ClearBuffers);

        WriteData(paramValue);
        WriteData(depthValue);
        WriteData(stencilValue);
        
        WriteBuffers();
        WCE();

        WaitForStep();
    }

    // Clear and set DEPTH2 for peeling
    virtual void PeelBuffers(u32 paramValue, float depthValue, u32 stencilValue) {
        backend->PeelBuffers(paramValue, depthValue, stencilValue);

        WCH(PeelBuffers);

        WriteData(paramValue);
        WriteData(depthValue);
        WriteData(stencilValue);

        WriteBuffers();
        WCE();

        WaitForStep();
    }

    // Summarize tile after rendering modvol (inside)
    virtual void SummarizeStencilOr() {
        backend->SummarizeStencilOr();

        WCH(SummarizeStencilOr);
        WriteBuffers();
        WCE();

        WaitForStep();
    }
    
    // Summarize tile after rendering modvol (outside)
    virtual void SummarizeStencilAnd() {

        backend->SummarizeStencilAnd();

        WCH(SummarizeStencilAnd);
        WriteBuffers();
        WCE();

        WaitForStep();
    }

    // Clear the pixel drawn counter
    virtual void ClearPixelsDrawn() {
        backend->ClearPixelsDrawn();

        WCH(ClearPixelsDrawn);

        WCE();

        WaitForStep();
    }

    // Get the pixel drawn counter. Used during layer peeling to determine when to stop processing
    virtual u32 GetPixelsDrawn() {
        auto rv = backend->GetPixelsDrawn();

        WCH(GetPixelsDrawn);

        WriteData(rv);

        WCE();

        WaitForStep();
        
        return rv;
    }

    void WriteData(DrawParameters* params)
    {
        WriteData(params->isp);
        WriteData(params->tsp);
        WriteData(params->tcw);
        WriteData(params->tsp2);
        WriteData(params->tcw2);
    }

    // Add an entry to the fpu parameters list
    virtual parameter_tag_t AddFpuEntry(DrawParameters* params, Vertex* vtx, RenderMode render_mode) {
        auto rv = backend->AddFpuEntry(params, vtx, render_mode);
        
        WCH(AddFpuEntry);

        WriteData(params);

        WriteData(vtx[0]);
        WriteData(vtx[1]);
        WriteData(vtx[2]);

        WriteData(render_mode);

        WriteData(rv);

        WCE();

        WaitForStep();

        return rv;
    }

    // Clear the fpu parameters list
    virtual void ClearFpuEntries() {
        backend->ClearFpuEntries();
        
        WCH(ClearFpuEntries);

        WCE();

        WaitForStep();
    }

    // Get the final output of the 32x32 tile. Used to write to the VRAM framebuffer
    virtual u8* GetColorOutputBuffer() {
        auto rv = backend->GetColorOutputBuffer();
        
        WCH(GetColorOutputBuffer);
        
        WriteBuffers();

        WCE();

        WaitForStep();

        return rv;
    }

    // Render to ACCUM from TAG buffer
    // TAG holds references to triangles, ACCUM is the tile framebuffer
    virtual void RenderParamTags(RenderMode rm, int tileX, int tileY) {
        backend->RenderParamTags(rm, tileX, tileY);

        WCH(RenderParamTags);
        
        WriteData(rm);
        WriteData(tileX);
        WriteData(tileY);

        WriteBuffers();
        WCE();

        WaitForStep();
    }

    // RasterizeTriangle
    virtual void RasterizeTriangle(RenderMode render_mode, DrawParameters* params, parameter_tag_t tag, int vertex_offset, const Vertex& v1, const Vertex& v2, const Vertex& v3, const Vertex* v4, taRECT* area) {
        backend->RasterizeTriangle(render_mode, params, tag, vertex_offset, v1, v2, v3, v4, area);

        WCH(RasterizeTriangle);
        
        WriteData(render_mode);
        WriteData(params);
        WriteData(tag);
        WriteData(vertex_offset);
        WriteData(v1);
        WriteData(v2);
        WriteData(v3);
        WriteData(v4 != nullptr);
        if (v4) {
            WriteData(*v4);
        }

        WriteData(area->left);
        WriteData(area->top);
        WriteData(area->right);
        WriteData(area->bottom);

        WriteBuffers();
        WCE();

        WaitForStep();
    }


    virtual u8* DebugGetAllBuffers() {
        die("bebugger inception?");
        
        return nullptr;
    }

    virtual ~RefRendDebug() {
        backend.reset();    //explicit for order of de-init
        CloseSocket();
        CloseServer();
    };
};

RefRendInterface*  rend_refred_debug(RefRendInterface* backend) {
    return new RefRendDebug(backend);
}