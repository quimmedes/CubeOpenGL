#include <windows.h>
#include <stdio.h>
#include <GL/glew.h>       // Biblioteca para carregar funções OpenGL (GLEW)
#include <GL/gl.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

// WGL constants for context creation
#ifndef WGL_CONTEXT_MAJOR_VERSION_ARB
#define WGL_CONTEXT_MAJOR_VERSION_ARB 0x2091
#endif
#ifndef WGL_CONTEXT_MINOR_VERSION_ARB
#define WGL_CONTEXT_MINOR_VERSION_ARB 0x2092
#endif
#ifndef WGL_CONTEXT_PROFILE_MASK_ARB
#define WGL_CONTEXT_PROFILE_MASK_ARB 0x9126
#endif
#ifndef WGL_CONTEXT_CORE_PROFILE_BIT_ARB
#define WGL_CONTEXT_CORE_PROFILE_BIT_ARB 0x00000001
#endif

// Global variables for window and OpenGL context
HDC hDCGlobal = NULL;
HGLRC hRCGlobal = NULL;

// Forward declaration do Window Proc
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

bool CreateModernContext(HWND hWnd)
{
    hDCGlobal = GetDC(hWnd);
    PIXELFORMATDESCRIPTOR pfd = { sizeof(pfd), 1 };
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.iLayerType = PFD_MAIN_PLANE;
    int pf = ChoosePixelFormat(hDCGlobal, &pfd);
    if (!pf) return false;
    if (!SetPixelFormat(hDCGlobal, pf, &pfd)) return false;

    // Cria um contexto temporário para carregar os ponteiros de função
    HGLRC tempContext = wglCreateContext(hDCGlobal);
    if (!tempContext) return false;
    if (!wglMakeCurrent(hDCGlobal, tempContext)) return false;

    // Obter o endereço da função wglCreateContextAttribsARB
    typedef HGLRC (WINAPI *PFNWGLCREATECONTEXTATTRIBSARBPROC)(HDC, HGLRC, const int*);
    PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB = 
        (PFNWGLCREATECONTEXTATTRIBSARBPROC)wglGetProcAddress("wglCreateContextAttribsARB");
    if (wglCreateContextAttribsARB)
    {
        int attribs[] =
        {
            WGL_CONTEXT_MAJOR_VERSION_ARB, 4,
            WGL_CONTEXT_MINOR_VERSION_ARB, 2,
            WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
            0,0
        };
        hRCGlobal = wglCreateContextAttribsARB(hDCGlobal, 0, attribs);
        if (!hRCGlobal) return false;
        wglMakeCurrent(NULL, NULL);
        wglDeleteContext(tempContext);
        if (!wglMakeCurrent(hDCGlobal, hRCGlobal)) return false;
    }
    else
    {
        hRCGlobal = tempContext;
    }
    return true;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
    // Registrar a classe de janela
    WNDCLASS wc = {};
    wc.style = CS_OWNDC;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"OpenGLWindowClass";
    if (!RegisterClass(&wc))
    {
        MessageBox(0, L"Falha ao registrar a classe da janela.", L"Erro", MB_OK | MB_ICONERROR);
        return -1;
    }

    // Criar a janela
    HWND hWnd = CreateWindow(
        L"OpenGLWindowClass",
        L"Cube OpenGL",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        1940,
        1080,
        nullptr,
        nullptr,
        hInstance,
        nullptr
    );
    if (!hWnd)
    {
        MessageBox(0, L"Falha ao criar a janela.", L"Erro", MB_OK | MB_ICONERROR);
        return -1;
    }

    // Cria o contexto OpenGL moderno (4.2 Core)
    if (!CreateModernContext(hWnd))
    {
        MessageBox(0, L"Falha ao criar o contexto OpenGL.", L"Erro", MB_OK | MB_ICONERROR);
        return -1;
    }

    // Inicializa GLEW após o contexto OpenGL estar ativo
    GLenum err = glewInit();
    if (err != GLEW_OK) {
        MessageBox(0, L"Falha ao inicializar GLEW.", L"Erro", MB_OK | MB_ICONERROR);
        return -1;
    }

    // Habilitar teste de profundidade
    glEnable(GL_DEPTH_TEST);

    // Desabilitar VSync (se possível)
    typedef BOOL (WINAPI *PFNWGLSWAPINTERVALEXTPROC)(int);
    PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT = (PFNWGLSWAPINTERVALEXTPROC)wglGetProcAddress("wglSwapIntervalEXT");
    if (wglSwapIntervalEXT) wglSwapIntervalEXT(0);

    // Definir código fonte dos shaders
    const char* vertexShaderSource =
        "#version 420 core\n"
        "layout (location = 0) in vec3 aPos;\n"
        "layout (location = 1) in vec3 aNormal;\n"
        "out vec3 Normal;\n"
        "out vec3 FragPos;\n"
        "uniform mat4 model;\n"
        "uniform mat4 view;\n"
        "uniform mat4 projection;\n"
        "void main(){\n"
        "   FragPos = vec3(model * vec4(aPos, 1.0));\n"
        "   Normal = mat3(transpose(inverse(model))) * aNormal;\n"
        "   gl_Position = projection * view * model * vec4(aPos, 1.0);\n"
        "}\n";

    const char* fragmentShaderSource =
        "#version 420 core\n"
        "in vec3 Normal;\n"
        "in vec3 FragPos;\n"
        "out vec4 FragColor;\n"
        "void main(){\n"
        "   vec3 norm = normalize(Normal);\n"
        "   vec3 lightDir = normalize(vec3(0.5, 1.0, 0.3));\n"
        "   float diff = max(dot(norm, lightDir), 0.0);\n"
        "   vec3 diffuse = diff * vec3(1.0, 0.8, 0.6);\n"
        "   vec3 ambient = vec3(0.2, 0.2, 0.2);\n"
        "   FragColor = vec4(Normal,1.0) +  vec4(diffuse + ambient, 1.0);\n"
        "}\n";

    // Compilar vertex shader
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, nullptr);
    glCompileShader(vertexShader);
    // (Adicionar verificação de erros em produção)

    // Compilar fragment shader
    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, nullptr);
    glCompileShader(fragmentShader);
    // (Adicionar verificação de erros em produção)

    // Linkar shaders em um programa
    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    // (Adicionar verificação de erros em produção)
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    // Carregar apenas arquivos específicos
    const char* meshFiles[] = {
        "cavalry.glb",
        "mulher.obj",
        "SK_HornedKnight_F_01.fbx",
        "teste.fbx",
        "cenario.fbx"
    };
    const int meshFileCount = sizeof(meshFiles) / sizeof(meshFiles[0]);
    // Configuração de transformação para cada mesh
    struct MeshTransform {
        glm::vec3 position;
        glm::vec3 rotation; // em graus
        glm::vec3 scale;
    };
    MeshTransform meshTransforms[] = {
        { glm::vec3(0,0,0), glm::vec3(0,0,0), glm::vec3(1,1,1) }, // cavalry.glb
        { glm::vec3(2,0,0), glm::vec3(0,0,0), glm::vec3(1,1,1) }, // mulher.obj
        { glm::vec3(0,0,-6), glm::vec3(0,0,0), glm::vec3(0.01f,0.01f,0.01f) }, // SK_HornedKnight_F_01.fbx
        { glm::vec3(0,2,0), glm::vec3(0,0,0), glm::vec3(1,1,1) }, // teste.fbx
        { glm::vec3(0,-2,0), glm::vec3(0,0,0), glm::vec3(2,2,2) } // cenario.fbx
        
    };
    std::vector<GLuint> vaos, ebos;
    std::vector<GLsizei> indexCounts;

    for (int fileIdx = 0; fileIdx < meshFileCount; ++fileIdx) {
        Assimp::Importer importer;
        const aiScene* scene = importer.ReadFile(meshFiles[fileIdx], aiProcess_Triangulate | aiProcess_GenNormals | aiProcess_JoinIdenticalVertices);
        if (!scene || !scene->HasMeshes()) continue;

        std::vector<float> vertices;
        std::vector<unsigned int> indices;
        unsigned int vertexOffset = 0;

        for (unsigned int m = 0; m < scene->mNumMeshes; ++m) {
            aiMesh* mesh = scene->mMeshes[m];
            for (unsigned int i = 0; i < mesh->mNumVertices; ++i) {
                vertices.push_back(mesh->mVertices[i].x);
                vertices.push_back(mesh->mVertices[i].y);
                vertices.push_back(mesh->mVertices[i].z);
                if (mesh->HasNormals()) {
                    vertices.push_back(mesh->mNormals[i].x);
                    vertices.push_back(mesh->mNormals[i].y);
                    vertices.push_back(mesh->mNormals[i].z);
                } else {
                    vertices.push_back(0.0f);
                    vertices.push_back(0.0f);
                    vertices.push_back(1.0f);
                }
            }
            for (unsigned int i = 0; i < mesh->mNumFaces; ++i) {
                const aiFace& face = mesh->mFaces[i];
                for (unsigned int j = 0; j < face.mNumIndices; ++j) {
                    indices.push_back(vertexOffset + face.mIndices[j]);
                }
            }
            vertexOffset += mesh->mNumVertices;
        }

        GLuint VAO, VBO, EBO;
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);
        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);

        vaos.push_back(VAO);
        ebos.push_back(EBO);
        indexCounts.push_back((GLsizei)indices.size());
    }

    // Variáveis da câmera
    glm::vec3 cameraPos = glm::vec3(0.0f, 0.0f, 10.0f);
    glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
    float yaw = -90.0f, pitch = 0.0f;
    POINT lastMousePos;
    bool firstMouse = true;
    float cameraSpeed = 30.0f;
    LARGE_INTEGER freq, prevFrameTime, currentFrameTime;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&prevFrameTime);

    // FPS
    DWORD lastTime = GetTickCount64();
    int frameCount = 0;
    double fps = 0.0f;
    char fpsText[64] = "";

    // Esconde cursor e captura mouse
    RECT winRect;
    ShowCursor(FALSE);
    GetClientRect(hWnd, &winRect);
    POINT center;
    center.x = (winRect.right - winRect.left) / 2;
    center.y = (winRect.bottom - winRect.top) / 2;
    ClientToScreen(hWnd, &center);
    SetCursorPos(center.x, center.y);
    lastMousePos = center;

    // Loop principal
    MSG msg = {};
    float angle = 0.0f;
    while (msg.message != WM_QUIT)
    {
        QueryPerformanceCounter(&currentFrameTime);
        double deltaTime = double(currentFrameTime.QuadPart - prevFrameTime.QuadPart) / freq.QuadPart;
        prevFrameTime = currentFrameTime;

        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_MOUSEMOVE)
            {
                POINT p;
                GetCursorPos(&p);
                RECT winRect;
                GetClientRect(hWnd, &winRect);
                POINT center;
                center.x = (winRect.right - winRect.left) / 2;
                center.y = (winRect.bottom - winRect.top) / 2;
                ClientToScreen(hWnd, &center);

                float xoffset = float(p.x - center.x);
                float yoffset = float(center.y - p.y);

                if (!firstMouse) {
                    float sensitivity = 0.15f;
                    xoffset *= sensitivity;
                    yoffset *= sensitivity;

                    yaw += xoffset;
                    pitch += yoffset;
                    if (pitch > 89.0f) pitch = 89.0f;
                    if (pitch < -89.0f) pitch = -89.0f;

                    glm::vec3 front;
                    front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
                    front.y = sin(glm::radians(pitch));
                    front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
                    cameraFront = glm::normalize(front);
                }
                firstMouse = false;
                SetCursorPos(center.x, center.y);
            }
            // Movimento suave com base no deltaTime
            SHORT w = GetAsyncKeyState('W');
            SHORT s = GetAsyncKeyState('S');
            SHORT a = GetAsyncKeyState('A');
            SHORT d = GetAsyncKeyState('D');
            glm::vec3 move(0.0f);
            if (w & 0x8000)
                move += cameraFront * cameraSpeed * (float)deltaTime;
            if (s & 0x8000)
                move -= cameraFront * cameraSpeed * (float)deltaTime;
            if (a & 0x8000)
                move -= glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed * (float)deltaTime;
            if (d & 0x8000)
                move += glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed * (float)deltaTime;
            SHORT e = GetAsyncKeyState('E');
            SHORT q = GetAsyncKeyState('Q');
            if (e & 0x8000)
                move += cameraUp * cameraSpeed * (float)deltaTime;
            if (q & 0x8000)
                move -= cameraUp * cameraSpeed * (float)deltaTime;
            if (glm::length(move) > 0.0f)
                cameraPos += move;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        angle += 0.01f;

        glm::mat4 model = glm::mat4(1.0f);
        glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
        glm::mat4 projection = glm::perspective(glm::radians(60.0f), 1940.0f/1080.0f, 0.1f, 1000.0f);

        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(shaderProgram);
        GLint modelLoc = glGetUniformLocation(shaderProgram, "model");
        GLint viewLoc  = glGetUniformLocation(shaderProgram, "view");
        GLint projLoc  = glGetUniformLocation(shaderProgram, "projection");
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));


        meshTransforms[0].position.x += 0.5f * (float)deltaTime; // Atualiza a posição da mesh cavalry.glb
        meshTransforms[1].position.x -= 0.5f * (float)deltaTime; // Atualiza a posição da mesh mulher.obj	



        for (size_t i = 0; i < vaos.size(); ++i) {

          
            
            glm::mat4 meshModel = model;
            meshModel = glm::translate(meshModel, meshTransforms[i].position);
            meshModel = glm::rotate(meshModel, glm::radians(meshTransforms[i].rotation.x), glm::vec3(1,0,0));
            meshModel = glm::rotate(meshModel, glm::radians(meshTransforms[i].rotation.y), glm::vec3(0,1,0));
            meshModel = glm::rotate(meshModel, glm::radians(meshTransforms[i].rotation.z), glm::vec3(0,0,1));
            meshModel = glm::scale(meshModel, meshTransforms[i].scale);
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(meshModel));
            glBindVertexArray(vaos[i]);
            glDrawElements(GL_TRIANGLES, indexCounts[i], GL_UNSIGNED_INT, 0);
        }

        // FPS calculation
        frameCount++;
        DWORD now = GetTickCount64();
        if (now - lastTime >= 1000) {
            fps = frameCount * 1000.0f / double(now - lastTime);
            frameCount = 0;
            lastTime = now;
            sprintf(fpsText, "FPS: %.1f", fps);
            SetWindowTextA(hWnd, fpsText);
        }

        // Desenhar FPS no canto superior esquerdo (GDI)
        HDC hdc = GetDC(hWnd);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(255,255,0));
        TextOutA(hdc, 10, 10, fpsText, (int)strlen(fpsText));
        ReleaseDC(hWnd, hdc);

        SwapBuffers(hDCGlobal);
    }
    ShowCursor(TRUE);

    // Libera recursos
    for (size_t i = 0; i < vaos.size(); ++i) {
        glDeleteVertexArrays(1, &vaos[i]);
        glDeleteBuffers(1, &ebos[i]);
    }
    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(hRCGlobal);
    return 0;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch(message)
    {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
    }
}
