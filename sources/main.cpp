#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include <d3d11.h>
#include <tchar.h>
#include <iostream>
#include <windows.h>
#include <chrono>
#include <thread>
#include <iomanip>
#include <iphlpapi.h>
#include <dwmapi.h>
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "dwmapi.lib")

class ramInfo
{
public:
    DWORDLONG total;
    DWORDLONG available;
    DWORDLONG used;
};
class cpuInfo
{
public:
    ULARGE_INTEGER idleTime;
    ULARGE_INTEGER kernelTime;
    ULARGE_INTEGER userTime;
};
class netInfo
{
public:
    unsigned long long inBytes;
    unsigned long long outBytes;
};
ramInfo getRamInfo()
{
    MEMORYSTATUSEX memStat;
    memStat.dwLength = sizeof(memStat);
    GlobalMemoryStatusEx(&memStat);
    ramInfo ram;
    ram.total = memStat.ullTotalPhys;
    ram.available = memStat.ullAvailPhys;
    ram.used = ram.total - ram.available;
    return ram;
}
cpuInfo getCpuInfo()
{
    FILETIME kerneltime, idletime, usertime;
    GetSystemTimes(&idletime, &kerneltime, &usertime);
    cpuInfo cpu;
    cpu.idleTime.LowPart = idletime.dwLowDateTime;
    cpu.idleTime.HighPart = idletime.dwHighDateTime;
    cpu.kernelTime.LowPart = kerneltime.dwLowDateTime;
    cpu.kernelTime.HighPart = kerneltime.dwHighDateTime;
    cpu.userTime.LowPart = usertime.dwLowDateTime;
    cpu.userTime.HighPart = usertime.dwHighDateTime;
    return cpu;
}
double getCpuUsage(const cpuInfo& previous, const cpuInfo& current)
{
    ULONGLONG kernelDiff = current.kernelTime.QuadPart - previous.kernelTime.QuadPart;
    ULONGLONG idleDiff = current.idleTime.QuadPart - previous.idleTime.QuadPart;
    ULONGLONG userDiff = current.userTime.QuadPart - previous.userTime.QuadPart;
    ULONGLONG totalDiff = kernelDiff + userDiff;
    if (totalDiff == 0)
    {
        return 0.0;
    }

    double usage = (totalDiff - idleDiff) * 100.0 / totalDiff;
    if (usage > 100)
    {
        return 100;
    }
    return usage;
}
double getGhz()
{
    unsigned __int64 startCycle = __rdtsc();
    auto startTime = std::chrono::high_resolution_clock::now();
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    unsigned __int64 endCycle = __rdtsc();
    auto endTime = std::chrono::high_resolution_clock::now();

    double duration = std::chrono::duration<double>(endTime - startTime).count();
    double cycle = static_cast<double>(endCycle - startCycle);
    return (cycle / duration) / 1000000000;
}
netInfo getNetInfo()
{
    MIB_IFTABLE* table;
    DWORD size = 0;
    GetIfTable(NULL, &size, FALSE);
    table = (MIB_IFTABLE*)malloc(size);
    GetIfTable(table, &size, FALSE);

    netInfo info;
    info.inBytes = 0;
    info.outBytes = 0;

    for (DWORD i = 0; i < table->dwNumEntries; i++)
    {
        info.inBytes += table->table[i].dwInOctets;
        info.outBytes += table->table[i].dwOutOctets;
    }

    free(table);
    return info;
}
void refreshInfo(ramInfo& ram, cpuInfo& currentCpu, double& cpuUsage, cpuInfo& previousCpu, double& ghz, bool& running, double& downloadMbps, double& uploadMbps, netInfo& previousNet)
{
    while (running)
    {
        ram = getRamInfo();
        currentCpu = getCpuInfo();
        cpuUsage = getCpuUsage(previousCpu, currentCpu);
        previousCpu = currentCpu;
        ghz = getGhz();
        netInfo currentNet = getNetInfo();
        unsigned long long downDiff = currentNet.inBytes - previousNet.inBytes;
        unsigned long long upDiff = currentNet.outBytes - previousNet.outBytes;
        previousNet = currentNet;
        downloadMbps = downDiff / 125000.0;
        uploadMbps = upDiff / 125000.0;
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
}
void SetDarkModeTitleBar(HWND hwnd, bool enable)
{
    // DWMWA_USE_IMMERSIVE_DARK_MODE = 20 or 19 depending on Windows version
    const DWORD DWMWA_USE_IMMERSIVE_DARK_MODE = 20;
    BOOL value = enable ? TRUE : FALSE;
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &value, sizeof(value));
}


static ID3D11Device*            g_pd3dDevice = nullptr;
static ID3D11DeviceContext*     g_pd3dDeviceContext = nullptr;
static IDXGISwapChain*          g_pSwapChain = nullptr;
static bool                     g_SwapChainOccluded = false;
static UINT                     g_ResizeWidth = 0, g_ResizeHeight = 0;
static ID3D11RenderTargetView*  g_mainRenderTargetView = nullptr;


bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

void myStyle() {
    ImGuiStyle& style = ImGui::GetStyle();
    style.Colors[ImGuiCol_WindowBg] = ImVec4(25.0f / 255, 30.0f / 255, 31.0f / 255, 255);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(9.0f / 255, 9.0f / 255, 10.0f / 255, 255);
    style.Colors[ImGuiCol_PlotHistogram] = ImVec4(150.0f / 255, 117.0f / 255, 122.0f / 255, 255);
   
    
}

int main(int, char**)
{
    cpuInfo previousCpu = getCpuInfo();
    netInfo previousNet = getNetInfo();
    ramInfo ram;
    cpuInfo currentCpu;
    double cpuUsage;
    double ghz;
    double downloadMbps = 0;
    double uploadMbps = 0;
    bool running = true;
    std::thread refresh(refreshInfo, std::ref(ram), std::ref(currentCpu), std::ref(cpuUsage), std::ref(previousCpu), std::ref(ghz), std::ref(running), std::ref(downloadMbps), std::ref(uploadMbps), std::ref(previousNet));
   
    ImGui_ImplWin32_EnableDpiAwareness();
    float main_scale = ImGui_ImplWin32_GetDpiScaleForMonitor(::MonitorFromPoint(POINT{ 0, 0 }, MONITOR_DEFAULTTOPRIMARY));

    
    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"ImGui Example", nullptr };
    ::RegisterClassExW(&wc);
    HWND hwnd = ::CreateWindowW(
        wc.lpszClassName,
        L"Resource Monitor",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        100, 100,
        (int)(410 * main_scale), (int)(370 * main_scale),
        nullptr, nullptr, wc.hInstance, nullptr);

    if (!CreateDeviceD3D(hwnd))
    {
        CleanupDeviceD3D();
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    
    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    SetDarkModeTitleBar(hwnd, true);
    ::UpdateWindow(hwnd);

    
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      

    
    ImGui::StyleColorsDark();
    
    
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(main_scale);        
    style.FontScaleDpi = main_scale;        

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    ImFont* font1vSmall = io.Fonts->AddFontFromFileTTF("C://Windows//Fonts//consola.ttf", 11.0f);
    ImFont* font2 = io.Fonts->AddFontFromFileTTF("C://Windows//Fonts//arial.ttf", 15.0f);
    ImFont* font1Small = io.Fonts->AddFontFromFileTTF("C://Windows//Fonts//consola.ttf", 15.0f);
    ImFont* font1Med = io.Fonts->AddFontFromFileTTF("C://Windows//Fonts//consola.ttf", 17.0f);
    ImFont* font1Big = io.Fonts->AddFontFromFileTTF("C://Windows//Fonts//consola.ttf", 18.0f);
    if (font1vSmall == nullptr) {
        std::cerr << "Error loading font" << '\n';
    }
    if (font2 == nullptr) {
        std::cerr << "Error loading font" << '\n';
    }
    
    bool show_demo_window = false;
    bool show_another_window = false;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    
    bool done = false;
    bool opened = true;
    while (!done)
    {

        
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done)
            break;

        
        if (g_SwapChainOccluded && g_pSwapChain->Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED)
        {
            ::Sleep(10);
            continue;
        }
        g_SwapChainOccluded = false;

        
        if (g_ResizeWidth != 0 && g_ResizeHeight != 0)
        {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, g_ResizeWidth, g_ResizeHeight, DXGI_FORMAT_UNKNOWN, 0);
            g_ResizeWidth = g_ResizeHeight = 0;
            CreateRenderTarget();
        }

       
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        static bool firstFrame = true;
        if (firstFrame)
        {
            firstFrame = false;

            RECT rc;
            GetClientRect(hwnd, &rc);
            float clientWidth = (float)(rc.right - rc.left);
            float clientHeight = (float)(rc.bottom - rc.top);

            ImVec2 windowSize = ImVec2(400, 380); 

            ImVec2 windowPos = ImVec2(
                (clientWidth - windowSize.x) * 0.5f,
                (clientHeight - windowSize.y) * 0.5f
            );

            ImGui::SetNextWindowPos(windowPos);
            ImGui::SetNextWindowSize(windowSize);
        }

        ImGui::PushFont(font2);
        float ramRatio = (float)ram.used / (float)ram.total;
        float cpuRatio = (float)cpuUsage / 100.0f;
        myStyle();
        ImGui::SetNextWindowSize(ImVec2(400, 360));
        if (ImGui::Begin("Resource Monitor", &opened, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove)) {
            ImGui::PopFont();
            ImGui::PushFont(font1Big);
            ImGui::Dummy(ImVec2(0.0f, 10.0f));
            ImGui::Text("Performance\n");
            ImGui::Dummy(ImVec2(0.0f, 10.0f));
            ImGui::PopFont();
            ImGui::PushFont(font1Med);
            ImGui::Separator();
            ImGui::TextColored(ImVec4(98.0f / 255, 196.0f / 255, 146.0f / 255, 255), "CPU");
            ImGui::PopFont();
            ImGui::PushFont(font1Small);
            ImGui::Text("Utilization:");
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(32.0f / 255, 166.0f / 255, 96.0f / 255, 255), "%.2f", cpuUsage);
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(110.0f / 255, 119.0f / 255, 145.0f / 255, 255), "%%");
            ImGui::Text("Base clock :");
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(105.0f / 255, 166.0f / 255, 31.0f / 255, 255), "%.2f", ghz);
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(110.0f / 255, 119.0f / 255, 145.0f / 255, 255), "Ghz");
            ImGui::PopFont();
         
            
            ImGui::Separator();
            ImGui::PushFont(font1Med);
            ImGui::TextColored(ImVec4(110.0f/255, 191.0f/255, 245.0f/255, 255),"Memory");
            ImGui::PopFont();
            ImGui::PushFont(font1Small);
            ImGui::Text("Used: ");
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(99.0f / 255, 99.0f / 255, 230.0f / 255, 255), "%.2f", ram.used / (1024.0 * 1024.0 * 1024.0));
            ImGui::SameLine();
            ImGui::Text("/");
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(116.0f / 255, 143.0f / 255, 212.0f / 255, 255), "%.2f", ram.total / (1024.0 * 1024.0 * 1024.0));
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(110.0f/255, 119.0f/255, 145.0f/255, 255),"Gbs");
            ImGui::Text("Total memory:");
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(25.0f / 255, 125.0f / 255, 156.0f / 255, 255), "%.0f", ram.total / (1024.0 * 1024.0));
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(110.0f / 255, 119.0f / 255, 145.0f / 255, 255), "MBs");
            ImGui::PopFont();


            ImGui::Separator();
            ImGui::PushFont(font1Med);
            ImGui::TextColored(ImVec4(186.0f / 255, 82.0f / 255, 122.0f / 255, 255), "Network");
            ImGui::PopFont();
            ImGui::PushFont(font1Small);
            ImGui::Text("Download:");
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(147.0f / 255, 124.0f / 255, 191.0f / 255, 255), "%.2f", downloadMbps);
            
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(110.0f / 255, 119.0f / 255, 145.0f / 255, 255), "Mbps");
            ImGui::Text("Upload  :");
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(171.0f / 255, 82.0f / 255, 186.0f / 255, 255), "%.2f", uploadMbps);
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(110.0f / 255, 119.0f / 255, 145.0f / 255, 255), "Mbps");
            ImGui::PopFont();
            ImGui::Separator();

            ImGui::Dummy(ImVec2(0.0f, 10.0f));

            ImGui::PushFont(font1vSmall);
            ImGui::Text("  Memory usage");
            ImGui::SameLine();
           
            ImGui::Text("                     %% CPU Utilization");
            ImGui::Dummy(ImVec2(0, 0));
            ImGui::SameLine(0, 10);
            ImGui::ProgressBar(ramRatio, ImVec2(180, 30));
            ImGui::SameLine(0,30);
           
            ImGui::PlotHistogram("###", &cpuRatio, 1, 0, nullptr, 0.0f, 1.0f, ImVec2(150, 30));
            ImGui::PopFont();
        }
        
        
        ImGui::End();

        

        

        // Rendering
        ImGui::Render();
        const float clear_color_with_alpha[4] = { clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        // Present
        HRESULT hr = g_pSwapChain->Present(1, 0);   // Present with vsync
        //HRESULT hr = g_pSwapChain->Present(0, 0); // Present without vsync
        g_SwapChainOccluded = (hr == DXGI_STATUS_OCCLUDED);
    }

    // Cleanup
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);

    return 0;
}

// Helper functions

bool CreateDeviceD3D(HWND hWnd)
{
    // Setup swap chain
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    //createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
    HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res == DXGI_ERROR_UNSUPPORTED) // Try high-performance WARP software driver if hardware is not available.
        res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res != S_OK)
        return false;

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

void CreateRenderTarget()
{
    ID3D11Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

void CleanupRenderTarget()
{
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Win32 message handler
// You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
// - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
// - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
// Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED)
            return 0;
        g_ResizeWidth = (UINT)LOWORD(lParam); // Queue resize
        g_ResizeHeight = (UINT)HIWORD(lParam);
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}

