#include <windows.h>
#include <algorithm> // для std::max и std::min
#include "Main.h"

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
    const wchar_t CLASS_NAME[] = L"DirectX11 Window Class";
    
    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    
    RegisterClass(&wc);

    D3DApp* app = new D3DApp(nullptr);

    HWND hwnd = CreateWindowEx(
        0,
        CLASS_NAME,
        L"DirectX 11 Cube",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        800, 600,
        nullptr,
        nullptr,
        hInstance,
        app
    );

    if (hwnd == nullptr)
    {
        delete app;
        return 0;
    }

    app->SetWindowHandle(hwnd);
    if (!app->Initialize())
    {
        delete app;
        return 0;
    }

    ShowWindow(hwnd, nCmdShow);

    LARGE_INTEGER frequency;
    LARGE_INTEGER lastTime;
    LARGE_INTEGER currentTime;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&lastTime);

    MSG msg = {};
    while (true)
    {
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
            {
                delete app;
                return 0;
            }

            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        QueryPerformanceCounter(&currentTime);
        float deltaTime = static_cast<float>(currentTime.QuadPart - lastTime.QuadPart) / frequency.QuadPart;
        lastTime = currentTime;

        app->Update(deltaTime);
        app->Render();
    }

    return 0;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_SIZE:
    {
        D3DApp* app = reinterpret_cast<D3DApp*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
        if (app)
        {
            RECT rc;
            GetClientRect(hwnd, &rc);
            app->OnResize(rc.right - rc.left, rc.bottom - rc.top);
        }
        return 0;
    }

    case WM_KEYDOWN:
    {
        D3DApp* app = reinterpret_cast<D3DApp*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
        if (app)
        {
            const float moveSpeed = 0.1f;
            const float rotateSpeed = 0.05f;

            switch (wParam)
            {
            case VK_F1:
                app->ToggleDebugLayer();
                break;
            case VK_F2:
                app->ToggleShaderDebug();
                break;
            case 'W':
                app->MoveCamera(0.0f, 0.0f, moveSpeed);
                break;
            case 'S':
                app->MoveCamera(0.0f, 0.0f, -moveSpeed);
                break;
            case 'A':
                app->MoveCamera(-moveSpeed, 0.0f, 0.0f);
                break;
            case 'D':
                app->MoveCamera(moveSpeed, 0.0f, 0.0f);
                break;
            case 'Q':
                app->MoveCamera(0.0f, moveSpeed, 0.0f);
                break;
            case 'E':
                app->MoveCamera(0.0f, -moveSpeed, 0.0f);
                break;
            case VK_LEFT:
                app->RotateCamera(-rotateSpeed, 0.0f);
                break;
            case VK_RIGHT:
                app->RotateCamera(rotateSpeed, 0.0f);
                break;
            case VK_UP:
                app->RotateCamera(0.0f, rotateSpeed);
                break;
            case VK_DOWN:
                app->RotateCamera(0.0f, -rotateSpeed);
                break;
            }
        }
        return 0;
    }

    case WM_CREATE:
    {
        CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
        D3DApp* app = reinterpret_cast<D3DApp*>(pCreate->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
        return 0;
    }
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

D3DApp::D3DApp(HWND hwnd) : 
    m_hwnd(hwnd),
    m_width(0),
    m_height(0),
    m_debugLayerEnabled(false),
    m_shaderDebugEnabled(false),
    m_cameraPosition(0.0f, 2.0f, -5.0f),
    m_cameraTarget(0.0f, 0.0f, 0.0f),
    m_cameraPitch(0.0f),
    m_cameraYaw(0.0f),
    m_cameraDistance(5.0f)
{
    if (hwnd)
    {
        RECT rc;
        GetClientRect(hwnd, &rc);
        m_width = rc.right - rc.left;
        m_height = rc.bottom - rc.top;
    }
}

D3DApp::~D3DApp()
{
    if (m_deviceContext)
        m_deviceContext->ClearState();
}

bool D3DApp::Initialize()
{
    if (!m_hwnd)
        return false;

    RECT rc;
    GetClientRect(m_hwnd, &rc);
    m_width = rc.right - rc.left;
    m_height = rc.bottom - rc.top;

    if (!CreateDeviceAndSwapChain())
        return false;

    if (!CreateRenderTargetView())
        return false;

    if (!CreateDepthStencilView())
        return false;

    if (!CreateVertexBuffer())
        return false;

    if (!CreateIndexBuffer())
        return false;

    if (!CreateConstantBuffer())
        return false;

    if (!CreateShaders())
        return false;

    if (!CreateInputLayout())
        return false;

    if (!CreateAxisBuffer())
        return false;

    if (!CreateGridBuffer())
        return false;

    return true;
}

bool D3DApp::CreateDeviceAndSwapChain()
{
    UINT createDeviceFlags = 0;
    if (m_debugLayerEnabled)
        createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;

    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
    };

    DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
    swapChainDesc.BufferCount = 1;
    swapChainDesc.BufferDesc.Width = m_width;
    swapChainDesc.BufferDesc.Height = m_height;
    swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferDesc.RefreshRate.Numerator = 60;
    swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.OutputWindow = m_hwnd;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.Windowed = TRUE;

    D3D_FEATURE_LEVEL featureLevel;

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        createDeviceFlags,
        featureLevels,
        ARRAYSIZE(featureLevels),
        D3D11_SDK_VERSION,
        &swapChainDesc,
        m_swapChain.GetAddressOf(),
        m_device.GetAddressOf(),
        &featureLevel,
        m_deviceContext.GetAddressOf()
    );

    if (FAILED(hr))
        return false;

    if (m_debugLayerEnabled)
    {
        hr = m_device.As(&m_debug);
        if (SUCCEEDED(hr))
        {
            m_debug->SetFeatureMask(D3D11_DEBUG_FEATURE_FINISH_PER_RENDER_OP | 
                                  D3D11_DEBUG_FEATURE_PRESENT_PER_RENDER_OP |
                                  D3D11_DEBUG_FEATURE_ALWAYS_DISCARD_OFFERED_RESOURCE);
        }
    }

    return true;
}

bool D3DApp::CreateRenderTargetView()
{
    ComPtr<ID3D11Texture2D> backBuffer;
    HRESULT hr = m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), &backBuffer);
    if (FAILED(hr))
        return false;

    hr = m_device->CreateRenderTargetView(backBuffer.Get(), nullptr, m_renderTargetView.GetAddressOf());
    if (FAILED(hr))
        return false;

    SetDebugName(m_renderTargetView.Get(), L"Main Render Target View");
    return true;
}

bool D3DApp::CreateDepthStencilView()
{
    D3D11_TEXTURE2D_DESC depthStencilDesc = {};
    depthStencilDesc.Width = m_width;
    depthStencilDesc.Height = m_height;
    depthStencilDesc.MipLevels = 1;
    depthStencilDesc.ArraySize = 1;
    depthStencilDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthStencilDesc.SampleDesc.Count = 1;
    depthStencilDesc.SampleDesc.Quality = 0;
    depthStencilDesc.Usage = D3D11_USAGE_DEFAULT;
    depthStencilDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

    ComPtr<ID3D11Texture2D> depthStencilBuffer;
    HRESULT hr = m_device->CreateTexture2D(&depthStencilDesc, nullptr, depthStencilBuffer.GetAddressOf());
    if (FAILED(hr))
        return false;

    hr = m_device->CreateDepthStencilView(depthStencilBuffer.Get(), nullptr, m_depthStencilView.GetAddressOf());
    if (FAILED(hr))
        return false;

    SetDebugName(m_depthStencilView.Get(), L"Main Depth Stencil View");
    return true;
}

bool D3DApp::CreateVertexBuffer()
{
    // Создаем вершины куба с разными цветами для каждой грани
    Vertex vertices[] = {
        // Front face (красная)
        { XMFLOAT3(-0.5f, -0.5f, -0.5f), XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f) },
        { XMFLOAT3(-0.5f,  0.5f, -0.5f), XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f) },
        { XMFLOAT3( 0.5f,  0.5f, -0.5f), XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f) },
        { XMFLOAT3( 0.5f, -0.5f, -0.5f), XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f) },
        
        // Back face (синяя)
        { XMFLOAT3(-0.5f, -0.5f,  0.5f), XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f) },
        { XMFLOAT3( 0.5f, -0.5f,  0.5f), XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f) },
        { XMFLOAT3( 0.5f,  0.5f,  0.5f), XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f) },
        { XMFLOAT3(-0.5f,  0.5f,  0.5f), XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f) }
    };

    D3D11_BUFFER_DESC vertexBufferDesc = {};
    vertexBufferDesc.ByteWidth = sizeof(vertices);
    vertexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
    vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA vertexData = {};
    vertexData.pSysMem = vertices;

    HRESULT hr = m_device->CreateBuffer(&vertexBufferDesc, &vertexData, m_vertexBuffer.GetAddressOf());
    if (FAILED(hr))
    {
        OutputDebugStringA("Failed to create vertex buffer\n");
        return false;
    }

    SetDebugName(m_vertexBuffer.Get(), L"Cube Vertex Buffer");
    return true;
}

bool D3DApp::CreateIndexBuffer()
{
    WORD indices[] = {
        0, 1, 2, 0, 2, 3,    // Front face
        4, 5, 6, 4, 6, 7,    // Back face
        0, 4, 7, 0, 7, 1,    // Left face
        3, 2, 6, 3, 6, 5,    // Right face
        1, 7, 6, 1, 6, 2,    // Top face
        0, 3, 5, 0, 5, 4     // Bottom face
    };

    D3D11_BUFFER_DESC indexBufferDesc = {};
    indexBufferDesc.ByteWidth = sizeof(indices);
    indexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
    indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

    D3D11_SUBRESOURCE_DATA indexData = {};
    indexData.pSysMem = indices;

    HRESULT hr = m_device->CreateBuffer(&indexBufferDesc, &indexData, m_indexBuffer.GetAddressOf());
    if (FAILED(hr))
    {
        OutputDebugStringA("Failed to create index buffer\n");
        return false;
    }

    SetDebugName(m_indexBuffer.Get(), L"Cube Index Buffer");
    return true;
}

bool D3DApp::CreateConstantBuffer()
{
    D3D11_BUFFER_DESC constantBufferDesc = {};
    constantBufferDesc.ByteWidth = sizeof(ConstantBuffer);
    constantBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    constantBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

    HRESULT hr = m_device->CreateBuffer(&constantBufferDesc, nullptr, m_constantBuffer.GetAddressOf());
    if (FAILED(hr))
        return false;

    SetDebugName(m_constantBuffer.Get(), L"Constant Buffer");
    return true;
}

bool D3DApp::CreateShaders()
{
    UINT compileFlags = 0;
    if (m_shaderDebugEnabled)
        compileFlags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;

    ComPtr<ID3DBlob> vertexShaderBlob;
    ComPtr<ID3DBlob> pixelShaderBlob;
    ComPtr<ID3DBlob> errorBlob;

    const char* vertexShaderSource = R"(
        struct VS_INPUT {
            float3 Position : POSITION;
            float4 Color : COLOR;
        };

        struct VS_OUTPUT {
            float4 Position : SV_POSITION;
            float4 Color : COLOR;
        };

        cbuffer ConstantBuffer : register(b0) {
            matrix World;
            matrix View;
            matrix Projection;
        };

        VS_OUTPUT main(VS_INPUT input) {
            VS_OUTPUT output;
            float4 pos = float4(input.Position, 1.0f);
            pos = mul(pos, World);
            pos = mul(pos, View);
            output.Position = mul(pos, Projection);
            output.Color = input.Color;
            return output;
        }
    )";

    const char* pixelShaderSource = R"(
        struct PS_INPUT {
            float4 Position : SV_POSITION;
            float4 Color : COLOR;
        };

        float4 main(PS_INPUT input) : SV_TARGET {
            return input.Color;
        }
    )";

    HRESULT hr = D3DCompile(
        vertexShaderSource,
        strlen(vertexShaderSource),
        nullptr,
        nullptr,
        nullptr,
        "main",
        "vs_5_0",
        compileFlags,
        0,
        vertexShaderBlob.GetAddressOf(),
        errorBlob.GetAddressOf()
    );

    if (FAILED(hr))
    {
        if (errorBlob)
        {
            OutputDebugStringA((char*)errorBlob->GetBufferPointer());
        }
        return false;
    }

    hr = D3DCompile(
        pixelShaderSource,
        strlen(pixelShaderSource),
        nullptr,
        nullptr,
        nullptr,
        "main",
        "ps_5_0",
        compileFlags,
        0,
        pixelShaderBlob.GetAddressOf(),
        errorBlob.GetAddressOf()
    );

    if (FAILED(hr))
    {
        if (errorBlob)
        {
            OutputDebugStringA((char*)errorBlob->GetBufferPointer());
        }
        return false;
    }

    hr = m_device->CreateVertexShader(
        vertexShaderBlob->GetBufferPointer(),
        vertexShaderBlob->GetBufferSize(),
        nullptr,
        m_vertexShader.GetAddressOf()
    );

    if (FAILED(hr))
        return false;

    hr = m_device->CreatePixelShader(
        pixelShaderBlob->GetBufferPointer(),
        pixelShaderBlob->GetBufferSize(),
        nullptr,
        m_pixelShader.GetAddressOf()
    );

    if (FAILED(hr))
        return false;

    SetDebugName(m_vertexShader.Get(), L"Cube Vertex Shader");
    SetDebugName(m_pixelShader.Get(), L"Cube Pixel Shader");
    return true;
}

bool D3DApp::CreateInputLayout()
{
    D3D11_INPUT_ELEMENT_DESC inputElementDesc[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };

    ComPtr<ID3DBlob> vertexShaderBlob;
    HRESULT hr = D3DCompile(
        R"(
            struct VS_INPUT {
                float3 Position : POSITION;
                float4 Color : COLOR;
            };
            float4 main(VS_INPUT input) : SV_POSITION { return float4(input.Position, 1.0f); }
        )",
        strlen(R"(
            struct VS_INPUT {
                float3 Position : POSITION;
                float4 Color : COLOR;
            };
            float4 main(VS_INPUT input) : SV_POSITION { return float4(input.Position, 1.0f); }
        )"),
        nullptr,
        nullptr,
        nullptr,
        "main",
        "vs_5_0",
        0,
        0,
        vertexShaderBlob.GetAddressOf(),
        nullptr
    );

    if (FAILED(hr))
        return false;

    hr = m_device->CreateInputLayout(
        inputElementDesc,
        ARRAYSIZE(inputElementDesc),
        vertexShaderBlob->GetBufferPointer(),
        vertexShaderBlob->GetBufferSize(),
        m_inputLayout.GetAddressOf()
    );

    if (FAILED(hr))
        return false;

    SetDebugName(m_inputLayout.Get(), L"Cube Input Layout");
    return true;
}

bool D3DApp::CreateAxisBuffer()
{
    const float axisY = -0.5f; // Позиция осей по Y (на нижней грани куба)

    // Создаем вершины для координатных осей
    Vertex axisVertices[] = {
        // Ось X (красная)
        { XMFLOAT3(0.0f, axisY, 0.0f), XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f) },
        { XMFLOAT3(10.0f, axisY, 0.0f), XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f) },
        
        // Ось Y (зеленая)
        { XMFLOAT3(0.0f, axisY, 0.0f), XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) },
        { XMFLOAT3(0.0f, axisY + 10.0f, 0.0f), XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) },
        
        // Ось Z (синяя)
        { XMFLOAT3(0.0f, axisY, 0.0f), XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f) },
        { XMFLOAT3(0.0f, axisY, 10.0f), XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f) }
    };

    D3D11_BUFFER_DESC axisBufferDesc = {};
    axisBufferDesc.ByteWidth = sizeof(axisVertices);
    axisBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
    axisBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA axisData = {};
    axisData.pSysMem = axisVertices;

    HRESULT hr = m_device->CreateBuffer(&axisBufferDesc, &axisData, m_axisVertexBuffer.GetAddressOf());
    if (FAILED(hr))
    {
        OutputDebugStringA("Failed to create axis buffer\n");
        return false;
    }

    SetDebugName(m_axisVertexBuffer.Get(), L"Coordinate Axes Buffer");
    return true;
}

bool D3DApp::CreateGridBuffer()
{
    const int gridSize = 20; // Размер сетки 20x20
    const float gridSpacing = 1.0f; // Расстояние между линиями
    const float gridExtent = (gridSize * gridSpacing) / 2.0f; // Половина размера сетки
    const float gridY = -0.5f; // Позиция сетки по Y (на нижней грани куба)

    std::vector<Vertex> gridVertices;
    gridVertices.reserve((gridSize + 1) * 4); // Линии сетки + оси

    // Создаем линии сетки
    for (int i = 0; i <= gridSize; ++i)
    {
        float pos = -gridExtent + i * gridSpacing;
        XMFLOAT4 color = (i == gridSize / 2) ? XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f) : XMFLOAT4(0.2f, 0.2f, 0.2f, 1.0f);

        // Горизонтальные линии
        gridVertices.push_back({ XMFLOAT3(-gridExtent, gridY, pos), color });
        gridVertices.push_back({ XMFLOAT3(gridExtent, gridY, pos), color });

        // Вертикальные линии
        gridVertices.push_back({ XMFLOAT3(pos, gridY, -gridExtent), color });
        gridVertices.push_back({ XMFLOAT3(pos, gridY, gridExtent), color });
    }

    D3D11_BUFFER_DESC gridBufferDesc = {};
    gridBufferDesc.ByteWidth = static_cast<UINT>(gridVertices.size() * sizeof(Vertex));
    gridBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
    gridBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA gridData = {};
    gridData.pSysMem = gridVertices.data();

    HRESULT hr = m_device->CreateBuffer(&gridBufferDesc, &gridData, m_gridVertexBuffer.GetAddressOf());
    if (FAILED(hr))
    {
        OutputDebugStringA("Failed to create grid buffer\n");
        return false;
    }

    SetDebugName(m_gridVertexBuffer.Get(), L"Grid Buffer");
    return true;
}

void D3DApp::MoveCamera(float dx, float dy, float dz)
{
    // Вычисляем направление камеры
    XMVECTOR forward = XMVector3Normalize(XMVectorSubtract(
        XMLoadFloat3(&m_cameraTarget),
        XMLoadFloat3(&m_cameraPosition)
    ));
    
    // Вычисляем правый вектор
    XMVECTOR right = XMVector3Normalize(XMVector3Cross(
        XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f),
        forward
    ));
    
    // Вычисляем вектор вверх
    XMVECTOR up = XMVector3Cross(forward, right);
    
    // Перемещаем камеру
    XMVECTOR position = XMLoadFloat3(&m_cameraPosition);
    position = XMVectorAdd(position, XMVectorScale(right, dx));
    position = XMVectorAdd(position, XMVectorScale(up, dy));
    position = XMVectorAdd(position, XMVectorScale(forward, dz));
    XMStoreFloat3(&m_cameraPosition, position);
    
    // Перемещаем точку наблюдения
    XMVECTOR target = XMLoadFloat3(&m_cameraTarget);
    target = XMVectorAdd(target, XMVectorScale(right, dx));
    target = XMVectorAdd(target, XMVectorScale(up, dy));
    target = XMVectorAdd(target, XMVectorScale(forward, dz));
    XMStoreFloat3(&m_cameraTarget, target);
}

void D3DApp::RotateCamera(float dx, float dy)
{
    m_cameraYaw += dx;
    m_cameraPitch += dy;
    
    // Ограничиваем угол наклона
    const float maxPitch = XM_PIDIV2 - 0.1f;
    const float minPitch = -XM_PIDIV2 + 0.1f;
    m_cameraPitch = (m_cameraPitch > maxPitch) ? maxPitch : 
                   (m_cameraPitch < minPitch) ? minPitch : m_cameraPitch;
    
    // Вычисляем новую позицию камеры
    float x = m_cameraDistance * cosf(m_cameraPitch) * sinf(m_cameraYaw);
    float y = m_cameraDistance * sinf(m_cameraPitch);
    float z = m_cameraDistance * cosf(m_cameraPitch) * cosf(m_cameraYaw);
    
    m_cameraPosition = XMFLOAT3(x, y, z);
}

void D3DApp::Update(float deltaTime)
{
    static float rotationAngle = 0.0f;
    
    // Обновляем угол вращения (только вокруг оси Y)
    rotationAngle += deltaTime * 1.0f;  // Вращение со скоростью 1 радиан в секунду

    // Создаем матрицу мира с вращением только вокруг оси Y
    XMMATRIX rotationMatrix = XMMatrixRotationY(rotationAngle);
    
    // Создаем матрицу вида
    XMVECTOR eye = XMLoadFloat3(&m_cameraPosition);
    XMVECTOR at = XMLoadFloat3(&m_cameraTarget);
    XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    XMMATRIX view = XMMatrixLookAtLH(eye, at, up);
    
    // Создаем матрицу проекции
    XMMATRIX projection = XMMatrixPerspectiveFovLH(
        XM_PIDIV4,                                          // Поле зрения (45 градусов)
        static_cast<float>(m_width) / static_cast<float>(m_height), // Соотношение сторон
        0.1f,                                              // Ближняя плоскость отсечения
        100.0f                                             // Дальняя плоскость отсечения
    );

    // Заполняем константный буфер
    ConstantBuffer constantBuffer;
    constantBuffer.World = XMMatrixTranspose(rotationMatrix);
    constantBuffer.View = XMMatrixTranspose(view);
    constantBuffer.Projection = XMMatrixTranspose(projection);

    // Обновляем константный буфер
    m_deviceContext->UpdateSubresource(m_constantBuffer.Get(), 0, nullptr, &constantBuffer, 0, 0);
}

void D3DApp::Render()
{
    float clearColor[4] = { 0.1f, 0.1f, 0.1f, 1.0f }; // Темно-серый фон
    m_deviceContext->ClearRenderTargetView(m_renderTargetView.Get(), clearColor);
    m_deviceContext->ClearDepthStencilView(m_depthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

    // Set viewport
    D3D11_VIEWPORT viewport = {};
    viewport.Width = static_cast<float>(m_width);
    viewport.Height = static_cast<float>(m_height);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    m_deviceContext->RSSetViewports(1, &viewport);

    m_deviceContext->OMSetRenderTargets(1, m_renderTargetView.GetAddressOf(), m_depthStencilView.Get());
    m_deviceContext->IASetInputLayout(m_inputLayout.Get());

    // Рисуем сетку
    m_deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    m_deviceContext->IASetVertexBuffers(0, 1, m_gridVertexBuffer.GetAddressOf(), &stride, &offset);
    m_deviceContext->VSSetShader(m_vertexShader.Get(), nullptr, 0);
    m_deviceContext->VSSetConstantBuffers(0, 1, m_constantBuffer.GetAddressOf());
    m_deviceContext->PSSetShader(m_pixelShader.Get(), nullptr, 0);
    m_deviceContext->Draw(84, 0); // Рисуем все линии сетки

    // Рисуем координатные оси
    m_deviceContext->IASetVertexBuffers(0, 1, m_axisVertexBuffer.GetAddressOf(), &stride, &offset);
    m_deviceContext->Draw(6, 0); // Рисуем 6 вершин (3 линии)

    // Рисуем куб
    m_deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_deviceContext->IASetVertexBuffers(0, 1, m_vertexBuffer.GetAddressOf(), &stride, &offset);
    m_deviceContext->IASetIndexBuffer(m_indexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);
    m_deviceContext->DrawIndexed(36, 0, 0);

    m_swapChain->Present(1, 0);
}

void D3DApp::OnResize(int width, int height)
{
    if (m_width == width && m_height == height)
        return;

    m_width = width;
    m_height = height;

    m_deviceContext->OMSetRenderTargets(0, nullptr, nullptr);
    m_renderTargetView.Reset();
    m_depthStencilView.Reset();

    HRESULT hr = m_swapChain->ResizeBuffers(1, m_width, m_height, DXGI_FORMAT_R8G8B8A8_UNORM, 0);
    if (FAILED(hr))
        return;

    CreateRenderTargetView();
    CreateDepthStencilView();
}

void D3DApp::ToggleDebugLayer()
{
    m_debugLayerEnabled = !m_debugLayerEnabled;
    // Note: Debug layer can only be enabled at device creation time
}

void D3DApp::ToggleShaderDebug()
{
    m_shaderDebugEnabled = !m_shaderDebugEnabled;
    // Note: Shader debug flags can only be set at shader compilation time
}

void D3DApp::SetDebugName(ID3D11DeviceChild* resource, const std::wstring& name)
{
    if (resource)
    {
        resource->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<UINT>(name.size() * sizeof(wchar_t)), name.c_str());
    }
} 