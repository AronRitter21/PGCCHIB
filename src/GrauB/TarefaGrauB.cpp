#include <fstream>
#include <sstream>
#include <unordered_set>
#include <string>
#include <iostream>
#include <vector>
#include <windows.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

using namespace glm;
using namespace std;

// Shaders em GLSL (vertex e fragment shader)
const GLchar *vertexShaderSource = R"(
 #version 400
 layout (location = 0) in vec3 position;
 layout (location = 1) in vec2 texc;
 out vec2 tex_coord;
 uniform mat4 model;
 uniform mat4 projection;
 void main()
 {
    tex_coord = vec2(texc.s, 1.0 - texc.t);
    gl_Position = projection * model * vec4(position, 1.0);
 }
 )";

const GLchar *fragmentShaderSource = R"(
 #version 400
 in vec2 tex_coord;
 out vec4 color;
 uniform sampler2D tex_buff;
 uniform vec2 offsetTex;

 void main()
 {
     color = texture(tex_buff,tex_coord + offsetTex);
 }
 )";

// Variáveis globais para janela e tamanho
GLFWwindow* g_window = nullptr;
int g_gl_width = 800;
int g_gl_height = 600;

// Estrutura do personagem
struct Personagem
{
    GLuint VAO;
    vec3 position;
    vec3 dimensions;
    float ds, dt;
    int nAnimations, nFrames;
    int iAnimation, iFrame;
};

// Estrutura para montagem dos tiles do mapa
struct MontagemTiles
{
    int qtdSprites;
    int tileWidth, tileHeight;
    int linhas, colunas;
    vector<int> matrizTiles; // Mapa linearizado
};

// Tipos de tile possíveis
enum class Tipo
{
    Normal,
    Morre,
    NaoCaminhavel,
};

// Estrutura de um tile
struct Tile
{
    GLuint VAO;
    GLuint texID;
    int iTile;
    vec3 position;
    vec3 dimensions;
    float ds, dt;
    Tipo tipo;
};

// Estrutura de uma moeda
struct Coin
{
    int i, j;		
    bool collected; 
    GLuint texID;
};

// Variáveis globais do jogo
MontagemTiles montagemTiles;
vector<Tile> tileset;
int personagemPosX = 0, personagemPosY = 0;
GLuint WIDTH = 800, HEIGHT = 600;

vector<Coin> coins;
int moedasColetadas = 0;
int totalMoedas = 0;

// Declaração de funções
void key_callback(GLFWwindow *window, int key, int scancode, int action, int mode);
int setupShader();
int setupSprite(int nAnimations, int nFrames, float &ds, float &dt);
bool montaTiles(const string &arquivo, MontagemTiles &montagemTiles);
bool carregaTipo(const string &arquivo, vector<Tipo> &tipoTile);
int setupTile(int qtdSprites, float &ds, float &dt);
int loadTexture(string filePath, int &width, int &height);
void montaTerreno(GLuint shaderID, float x0, float y0);
void defineMoedas(const string &mapaPath, GLuint texCoin);
void colocaMoedas(GLuint shaderID, float x0, float y0, const vector<Coin> &coins);

int main()
{
    srand(glfwGetTime());

    string erros;
    // Carrega informações dos tiles do arquivo
    if (!montaTiles("../src/GrauB/tileVals.txt", montagemTiles))
    {
        return -1;
    }
    int margem = 40;
    // Calcula tamanho do mapa em pixels (isométrico)
    int mapaWidthPx  = (montagemTiles.colunas + montagemTiles.linhas) * montagemTiles.tileWidth  / 2;
    int mapaHeightPx = (montagemTiles.colunas + montagemTiles.linhas) * montagemTiles.tileHeight / 2;
    WIDTH = mapaWidthPx + margem;
    HEIGHT = mapaHeightPx + margem;
    // Posição inicial do mapa na tela
    float x0 = (montagemTiles.linhas - 1) * montagemTiles.tileWidth * 0.5f;
    float y0 = (HEIGHT - mapaHeightPx) / 2.0f;

    // Inicializa GLFW e cria janela
    glfwInit();
    glfwWindowHint(GLFW_SAMPLES, 8);
    GLFWwindow *window = glfwCreateWindow(WIDTH, HEIGHT, "Grau B", nullptr, nullptr);

    // Carrega tipos de tiles (Normal, Morre, NaoCaminhavel)
    vector<Tipo> tipoTile(montagemTiles.qtdSprites, Tipo::Normal);
    if (!carregaTipo("../src/GrauB/tileTipos.txt", tipoTile))
    {
        cerr << "Erro ao carregar tileTipos.txt" << endl;
        return -1;
    }

    if (!window)
    {
        std::cerr << "Falha ao criar a janela GLFW" << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSetKeyCallback(window, key_callback);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cerr << "Falha ao inicializar GLAD" << std::endl;
        return -1;
    }

    // Exibe informações do OpenGL
    const GLubyte *renderer = glGetString(GL_RENDERER);
    const GLubyte *version = glGetString(GL_VERSION);
    cout << "Renderer: " << renderer << endl;
    cout << "OpenGL version supported " << version << endl;

    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    glViewport(0, 0, width, height);

    // Compila e linka shaders
    GLuint shaderID = setupShader();

    // Carrega texturas dos tiles e das moedas
    int Width, Height;
    GLuint texID = loadTexture("../assets/tilesets/tilesetIso.png", Width, Height);
    GLuint texCoin = loadTexture("../assets/sprites/pila.png", Width, Height);

    // Cria todos os tiles do tileset
    for (int i = 0; i < montagemTiles.qtdSprites; ++i)
    {
        Tile tile;
        tile.dimensions = vec3(montagemTiles.tileWidth, montagemTiles.tileHeight, 1.0);
        tile.iTile = i;
        tile.tipo = tipoTile[i];
        tile.texID = texID;
        tile.VAO = setupTile(montagemTiles.qtdSprites, tile.ds, tile.dt);
        tileset.push_back(tile);
    }

    glUseProgram(shaderID);

    // Variáveis para controle de FPS e animação
    double prev_s = glfwGetTime();
    double title_countdown_s = 0.1;
    float colorValue = 0.0;
    glActiveTexture(GL_TEXTURE0);
    glUniform1i(glGetUniformLocation(shaderID, "tex_buff"), 0);
    mat4 projection = ortho(0.0f, (float)WIDTH, (float)HEIGHT, 0.0f, -1.0f, 1.0f);
    glUniformMatrix4fv(glGetUniformLocation(shaderID, "projection"), 1, GL_FALSE, value_ptr(projection));
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_ALWAYS);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Carrega textura e inicializa personagem
    GLuint texIDJogador = loadTexture("../assets/sprites/enemies-spritesheet1.png", Width, Height);
    Personagem jogador;
    jogador.dimensions = vec3(montagemTiles.tileWidth, montagemTiles.tileWidth, 1.0);
    jogador.nAnimations = 12;
    jogador.nFrames = 2;
    jogador.iAnimation = 10;
    jogador.iFrame = 0; 
    jogador.VAO = setupSprite(jogador.nAnimations, jogador.nFrames, jogador.ds, jogador.dt); 
    double atrasoTrocaFrame = 0;

    double lastTime = glfwGetTime();
    double deltaT = 0.0;
    personagemPosX = 5;
    personagemPosY = 5;
    defineMoedas("../src/GrauB/moedas.txt", texCoin);

    // Loop principal do jogo
    while (!glfwWindowShouldClose(window))
    {
        {
            // Atualiza título da janela periodicamente
            double curr_s = glfwGetTime();
            double elapsed_s = curr_s - prev_s;
            prev_s = curr_s;

            title_countdown_s -= elapsed_s;
            if (title_countdown_s <= 0.0 && elapsed_s > 0.0)
            {
                double fps = 1.0 / elapsed_s;

                char tmp[256];
                sprintf(tmp, "Áron Ritter - Grau B");
                glfwSetWindowTitle(window, tmp);
                title_countdown_s = 0.1;
            }
        }

        glfwPollEvents();

        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glLineWidth(10);
        glPointSize(20);

        // Controle de animação do personagem
        double currTime = glfwGetTime();
        deltaT = currTime - lastTime;
        lastTime = currTime;

        atrasoTrocaFrame += deltaT;
        int auxFrame = jogador.iFrame;
        if (atrasoTrocaFrame > 0.2) 
        {
            auxFrame++;
            if (auxFrame > 2)
                auxFrame = 0;
            jogador.iFrame = auxFrame;
            atrasoTrocaFrame = 0;
        }

        // Desenha o terreno e as moedas
        montaTerreno(shaderID, x0, y0);
        colocaMoedas(shaderID, x0, y0, coins);

        // Pega o tile atual do personagem
        Tile tileAtual = tileset[montagemTiles.matrizTiles[personagemPosX * montagemTiles.colunas + personagemPosY]];

        // Calcula posição isométrica do personagem
        float x = x0 + (personagemPosY - personagemPosX) * tileAtual.dimensions.x / 2.0f;
        float y = y0 + (personagemPosY + personagemPosX) * tileAtual.dimensions.y / 2.0f;

        // Monta matriz de transformação do personagem
        mat4 model = mat4(1.0);
        model = translate(model, vec3(x + tileAtual.dimensions.x / 2.0, y + tileAtual.dimensions.y / 2.0 - jogador.dimensions.y / 2.0, 0));
        model = scale(model, jogador.dimensions);

        glUniformMatrix4fv(glGetUniformLocation(shaderID, "model"), 1, GL_FALSE, value_ptr(model));

        // Define o recorte do frame do personagem na spritesheet
        vec2 offsetTex;
        offsetTex.s = jogador.iFrame * jogador.ds;
        offsetTex.t = 1.0 - jogador.dt;
        glUniform2f(glGetUniformLocation(shaderID, "offsetTex"), offsetTex.s, offsetTex.t);

        // Desenha o personagem
        glBindVertexArray(jogador.VAO);
        glBindTexture(GL_TEXTURE_2D, texIDJogador);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        // Verifica se a moeda foi coletada e adiciona pontuação
        for (auto &moeda : coins)
        {
            if (!moeda.collected && moeda.i == personagemPosX && moeda.j == personagemPosY)
            {
                moeda.collected = true;
                moedasColetadas++;
            }
        }

        // Verifica se todas as moedas foram coletadas
        if (moedasColetadas == totalMoedas && totalMoedas > 0)
        {
            cout << "Você ganhou!" << endl;
            glfwSetWindowShouldClose(window, GL_TRUE);
        }

        glfwSwapBuffers(window);
    }

    glfwTerminate();
    return 0;
}

// Função de callback para teclas (movimentação do personagem)
void key_callback(GLFWwindow *window, int key, int scancode, int action, int mode)
{
    int c = 0, r = 0;
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GL_TRUE);
    
    // Movimentação isométrica (WASD + QEZXC)
    if (key == GLFW_KEY_W && action == GLFW_PRESS) { c = -1; r = -1; }
    if (key == GLFW_KEY_S && action == GLFW_PRESS) { c = +1; r = +1; }
    if (key == GLFW_KEY_A && action == GLFW_PRESS) { c = +1; r = -1; }
    if (key == GLFW_KEY_D && action == GLFW_PRESS) { c = -1; r = +1; }
    if (key == GLFW_KEY_Z && action == GLFW_PRESS) { c = +1; r = 0; }
    if (key == GLFW_KEY_X && action == GLFW_PRESS) { c = 0; r = +1; }
    if (key == GLFW_KEY_Q && action == GLFW_PRESS) { c = 0; r = -1; }
    if (key == GLFW_KEY_E && action == GLFW_PRESS) { c = -1; r = 0; }

    int destinoI = personagemPosX + c;
    int destinoJ = personagemPosY + r;

    // Verifica se o movimento é válido e executa
    if (destinoI >= 0 && destinoI < montagemTiles.linhas && destinoJ >= 0 && destinoJ < montagemTiles.colunas)
    {
        int tileID = montagemTiles.matrizTiles[destinoI * montagemTiles.colunas + destinoJ];
        Tipo tipoTile = tileset[tileID].tipo;

        switch (tipoTile)
        {
        case Tipo::Normal:
            personagemPosX = destinoI;
            personagemPosY = destinoJ;
            break;
        case Tipo::NaoCaminhavel:
            break;
        case Tipo::Morre:
            cout << "Você caiu na lava e MORREU!" << endl;
            glfwSetWindowShouldClose(window, GL_TRUE);
            break;
        default:
            break;
        }
    }
}

// Compila e linka shaders
int setupShader()
{
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);
    GLint success;
    GLchar infoLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n"
                  << infoLog << std::endl;
    }
    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n"
                  << infoLog << std::endl;
    }
    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success)
    {
        glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n"
                  << infoLog << std::endl;
    }
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return shaderProgram;
}

// Cria VAO para sprites animadas (personagem, moedas)
int setupSprite(int nAnimations, int nFrames, float &ds, float &dt)
{
    ds = 1.0 / (float)nFrames;
    dt = 1.0 / (float)nAnimations;

    GLfloat vertices[] = {
        -0.5, 0.5, 0.0, 0.0, 0.0,
        -0.5, -0.5, 0.0, 0.0, dt,
        0.5, 0.5, 0.0, ds, 0.0,
        0.5, -0.5, 0.0, ds, dt};

    GLuint VBO, VAO;
    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glGenVertexArrays(1, &VAO);

    glBindVertexArray(VAO);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), (GLvoid *)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), (GLvoid *)(3 * sizeof(GLfloat)));
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glBindVertexArray(0);

    return VAO;
}

// Cria VAO para tiles isométricos
int setupTile(int qtdSprites, float &ds, float &dt)
{
    ds = 1.0 / (float)qtdSprites;
    dt = 1.0;

    float th = 1.0, tw = 1.0;

    GLfloat vertices[] = {
        0.0, th / 2.0f, 0.0, 0.0, dt / 2.0f,
        tw / 2.0f, th, 0.0, ds / 2.0f, dt,
        tw / 2.0f, 0.0, 0.0, ds / 2.0f, 0.0,
        tw, th / 2.0f, 0.0, ds, dt / 2.0f};

    GLuint VBO, VAO;
    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glGenVertexArrays(1, &VAO);

    glBindVertexArray(VAO);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), (GLvoid *)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), (GLvoid *)(3 * sizeof(GLfloat)));
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glBindVertexArray(0);

    return VAO;
}

// Carrega textura de arquivo
int loadTexture(string filePath, int &width, int &height)
{
    GLuint texID;

    glGenTextures(1, &texID);
    glBindTexture(GL_TEXTURE_2D, texID);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    int nrChannels;

    unsigned char *data = stbi_load(filePath.c_str(), &width, &height, &nrChannels, 0);

    if (data)
    {
        if (nrChannels == 3)
        {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
        }
        else
        {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
        }
        glGenerateMipmap(GL_TEXTURE_2D);
    }
    else
    {
        std::cout << "Failed to load texture" << std::endl;
    }

    stbi_image_free(data);

    glBindTexture(GL_TEXTURE_2D, 0);

    return texID;
}

// Desenha o terreno (tiles) na tela
void montaTerreno(GLuint shaderID, float x0, float y0)
{
    for (int i = 0; i < montagemTiles.linhas; i++)
    {
        for (int j = 0; j < montagemTiles.colunas; j++)
        {
            mat4 model = mat4(1);

            Tile tileAtual = tileset[montagemTiles.matrizTiles[i * montagemTiles.colunas + j]];
            if (i == personagemPosX && j == personagemPosY)
                tileAtual = tileset[6];

            float x = x0 + (j - i) * tileAtual.dimensions.x / 2.0f;
            float y = y0 + (j + i) * tileAtual.dimensions.y / 2.0f;

            model = translate(model, vec3(x, y, 0.0));
            model = scale(model, tileAtual.dimensions);
            glUniformMatrix4fv(glGetUniformLocation(shaderID, "model"), 1, GL_FALSE, value_ptr(model));

            vec2 offsetTex;

            offsetTex.s = tileAtual.iTile * tileAtual.ds;
            offsetTex.t = 0.0;
            glUniform2f(glGetUniformLocation(shaderID, "offsetTex"), offsetTex.s, offsetTex.t);

            glBindVertexArray(tileAtual.VAO);
            glBindTexture(GL_TEXTURE_2D, tileAtual.texID);

            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        }
    }
}

// Lê arquivo de moedas e inicializa vetor de moedas
void defineMoedas(const string &path, GLuint texCoin)
{
    coins.clear();
    ifstream file(path);
    int linhas, colunas;
    file >> linhas >> colunas; 
    int possuiCoin;
    
    for (int i = 0; i < linhas; ++i) {
        for (int j = 0; j < colunas; ++j) {
            file >> possuiCoin;
            if (possuiCoin == 1) {
                coins.push_back({i, j, false, texCoin});
            }
        }
    }
    totalMoedas = coins.size();
    cout << "Total de moedas: " << totalMoedas << endl;
}

// Desenha as moedas na tela
void colocaMoedas(GLuint shaderID, float x0, float y0, const vector<Coin> &coins)
{
    // Define tamanho e outras propriedades da moeda
    vec3 dimensoesMoeda = vec3(montagemTiles.tileWidth * 0.4f, montagemTiles.tileHeight * 0.9f, 1.0f);
    float ds = 1.0;
    float dt = 1.0;

    static GLuint coinVAO = setupSprite(1, 1, ds, dt); // Uma sprite estática (1x1)

    glBindVertexArray(coinVAO);

    for (const auto &moeda : coins)
    {
        if (moeda.collected)
            continue;

        Tile tileAtual = tileset[montagemTiles.matrizTiles[moeda.i * montagemTiles.colunas + moeda.j]];

        float x = x0 + (moeda.j - moeda.i) * tileAtual.dimensions.x / 2.0f;
        float y = y0 + (moeda.j + moeda.i) * tileAtual.dimensions.y / 2.0f;

        mat4 model = mat4(1.0);
        model = translate(model, vec3(x + tileAtual.dimensions.x / 2.0f, y + tileAtual.dimensions.y / 2.0f - dimensoesMoeda.y / 2.0f, 0.1f));
        model = scale(model, dimensoesMoeda);

        glUniformMatrix4fv(glGetUniformLocation(shaderID, "model"), 1, GL_FALSE, value_ptr(model));

        // Usando o canto superior esquerdo da textura da moeda
        glUniform2f(glGetUniformLocation(shaderID, "offsetTex"), 0.0f, 1.0f - dt);

        // IMPORTANTE: Vincula a textura da moeda!
        glBindTexture(GL_TEXTURE_2D, moeda.texID);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }

    glBindVertexArray(0);
}

// Lê arquivo de tiles e monta o mapa
bool montaTiles(const string &arquivo, MontagemTiles &montagemTiles)
{
    int v;
    vector<int> matrizTiposTiles;
    
    ifstream file(arquivo);
    file >> montagemTiles.qtdSprites >> montagemTiles.tileWidth >> montagemTiles.tileHeight
         >> montagemTiles.linhas >> montagemTiles.colunas;
 
    for (int i = 0; i < montagemTiles.linhas; ++i) {
        for (int j = 0; j < montagemTiles.colunas; ++j) {
            file >> v;
            matrizTiposTiles.push_back(v);
        }
    }

    montagemTiles.matrizTiles.swap(matrizTiposTiles);

    return true;
    
}

// Lê arquivo de tipos de tile e preenche vetor de tipos
bool carregaTipo(const string &arquivo, vector<Tipo> &tipoTile)
{
    int tipoCount = 0;
    int aux;
    ifstream file(arquivo);
    while (tipoCount < 3)
    {
        file >> aux;
        if (aux == 9){
            tipoCount++;
        } else {
            switch (tipoCount)
            {
                case 0:
                    tipoTile[aux] = Tipo::Normal;
                    break;
                case 1: 
                    tipoTile[aux] = Tipo::NaoCaminhavel;
                    break;
                case 2:
                    tipoTile[aux] = Tipo::Morre;
                    break;
            }
        }
    }
    return true;
}