#pragma once

#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <directxmath.h>
#include <wrl/client.h>
#include <string>
#include <vector>
#include <dxgidebug.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxguid.lib")

using namespace DirectX;
using Microsoft::WRL::ComPtr;

class D3DApp {
public:
    D3DApp(HWND hwnd);
    ~D3DApp();

    bool Initialize();
    void Update(float deltaTime);
    void Render();
    void OnResize(int width, int height);
    void ToggleDebugLayer();
    void ToggleShaderDebug();
    void SetWindowHandle(HWND hwnd) { m_hwnd = hwnd; }

    void MoveCamera(float dx, float dy, float dz);
    void RotateCamera(float dx, float dy);

private:
    bool CreateDeviceAndSwapChain();
    bool CreateRenderTargetView();
    bool CreateDepthStencilView();
    bool CreateVertexBuffer();
    bool CreateIndexBuffer();
    bool CreateConstantBuffer();
    bool CreateShaders();
    bool CreateInputLayout();
    bool CreateAxisBuffer();
    bool CreateGridBuffer();

    void SetDebugName(ID3D11DeviceChild* resource, const std::wstring& name);

    HWND m_hwnd;
    int m_width;
    int m_height;
    bool m_debugLayerEnabled;
    bool m_shaderDebugEnabled;

    XMFLOAT3 m_cameraPosition;
    XMFLOAT3 m_cameraTarget;
    float m_cameraPitch;
    float m_cameraYaw;
    float m_cameraDistance;

    ComPtr<ID3D11Device> m_device;
    ComPtr<ID3D11DeviceContext> m_deviceContext;
    ComPtr<IDXGISwapChain> m_swapChain;
    ComPtr<ID3D11RenderTargetView> m_renderTargetView;
    ComPtr<ID3D11DepthStencilView> m_depthStencilView;
    ComPtr<ID3D11Buffer> m_vertexBuffer;
    ComPtr<ID3D11Buffer> m_indexBuffer;
    ComPtr<ID3D11Buffer> m_constantBuffer;
    ComPtr<ID3D11Buffer> m_axisVertexBuffer;
    ComPtr<ID3D11Buffer> m_gridVertexBuffer;
    ComPtr<ID3D11VertexShader> m_vertexShader;
    ComPtr<ID3D11PixelShader> m_pixelShader;
    ComPtr<ID3D11InputLayout> m_inputLayout;
    ComPtr<ID3D11Debug> m_debug;

    struct Vertex {
        XMFLOAT3 Position;
        XMFLOAT4 Color;
    };

    struct ConstantBuffer {
        XMMATRIX World;
        XMMATRIX View;
        XMMATRIX Projection;
    };
}; 