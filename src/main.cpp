#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <learnopengl/filesystem.h>
#include <learnopengl/shader.h>
#include <learnopengl/camera.h>
#include <learnopengl/model.h>

#include <iostream>

void framebuffer_size_callback(GLFWwindow *window, int width, int height);
void mouse_callback(GLFWwindow *window, double xpos, double ypos);
void scroll_callback(GLFWwindow *window, double xoffset, double yoffset);
void processInput(GLFWwindow *window);
void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods);
unsigned int loadTexture(std::string pathToTex);
unsigned int loadCubemap(vector<std::string> faces);

struct DirLight {
    glm::vec3 direction;

    glm::vec3 ambient;
    glm::vec3 diffuse;
    glm::vec3 specular;
};

struct PointLight {
    glm::vec3 position;

    float constant;
    float linear;
    float quadratic;

    glm::vec3 ambient;
    glm::vec3 diffuse;
    glm::vec3 specular;
};

struct SpotLight {
    bool enabled = true;

    glm::vec3 position;
    glm::vec3 direction;
    float cutOff;
    float outerCutOff;

    float constant;
    float linear;
    float quadratic;

    glm::vec3 ambient;
    glm::vec3 diffuse;
    glm::vec3 specular;
};

// Screen
const unsigned int SCR_WIDTH = 1400;
const unsigned int SCR_HEIGHT = 800;

// Camera
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

// Timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;

struct ProgramState {
    glm::vec3 clearColor = glm::vec3(0);
    bool ImGuiEnabled = false;
    Camera camera;
    bool CameraMouseMovementUpdateEnabled = true;
    glm::vec3 housePosition = glm::vec3(100.0f, 0.0f, 0.0f);
    glm::vec3 pyramidPosition = housePosition + glm::vec3(-3.0f, 4.0f, 0.0f);
    glm::vec3 pyramidColor = glm::vec3(1.0f, 0.0f, 0.0f);
    float houseScale = 1.0f;
    DirLight dirLight;
    PointLight ptLight;
    SpotLight spotLight;
    bool blinn = true;
    bool randColor = false;
    ProgramState()
            : camera(glm::vec3(0.0f, 5.0f, 0.0f)) {}
    void SaveToFile(std::string filename);
    void LoadFromFile(std::string filename);
};

void ProgramState::SaveToFile(std::string filename) {
    std::ofstream out(filename);
    out << clearColor.r << '\n'
        << clearColor.g << '\n'
        << clearColor.b << '\n'
        << ImGuiEnabled << '\n'
        << camera.Position.x << '\n'
        << camera.Position.y << '\n'
        << camera.Position.z << '\n'
        << camera.Front.x << '\n'
        << camera.Front.y << '\n'
        << camera.Front.z << '\n'
        << pyramidColor.x << '\n'
        << pyramidColor.y << '\n'
        << pyramidColor.z << '\n';
}

void ProgramState::LoadFromFile(std::string filename) {
    std::ifstream in(filename);
    if (in) {
        in >> clearColor.r
           >> clearColor.g
           >> clearColor.b
           >> ImGuiEnabled
           >> camera.Position.x
           >> camera.Position.y
           >> camera.Position.z
           >> camera.Front.x
           >> camera.Front.y
           >> camera.Front.z
           >> pyramidColor.x
           >> pyramidColor.y
           >> pyramidColor.z;
    }
}

ProgramState *programState;

void DrawImGui(ProgramState *programState);

int main() {
    // Initialization
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // Window creation
    GLFWwindow *window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Scene", NULL, NULL);
    if (window == NULL) {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetKeyCallback(window, key_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // Load all OpenGL function pointers
    if (!gladLoadGLLoader((GLADloadproc) glfwGetProcAddress)) {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    // Tell stb_image.h to flip loaded texture's on the y-axis (before loading model).
    stbi_set_flip_vertically_on_load(true);

    programState = new ProgramState;
    programState->LoadFromFile("resources/program_state.txt");
    if (programState->ImGuiEnabled) {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
    // Init Imgui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void) io;

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330 core");

    // Configure global opengl state
    glEnable(GL_DEPTH_TEST);

    // Shaders
    Shader modelShader("resources/shaders/model_shader.vs", "resources/shaders/model_shader.fs");
    Shader lightShader("resources/shaders/light_shader.vs", "resources/shaders/light_shader.fs");
    Shader skyboxShader("resources/shaders/skybox_shader.vs", "resources/shaders/skybox_shader.fs");
    Shader terrainShader("resources/shaders/terrain_shader.vs", "resources/shaders/terrain_shader.fs");

    // House model
    Model house("resources/objects/house/highpoly_town_house_01.obj");
    house.SetShaderTextureNamePrefix("material.");

    // Pyramid setup
    float pyramidVertices[] = {
            // positions
            0.0f, -0.5f, 0.0f,
            -0.5f, -0.5f, -0.5f,
            0.5f, -0.5f, -0.5f,

            0.0f, -0.5f, 0.0f,
            0.5f, -0.5f, -0.5f,
            0.5f, -0.5f, 0.5f,

            0.0f, -0.5f, 0.0f,
            0.5f, -0.5f, 0.5f,
            -0.5f, -0.5f, 0.5f,

            0.0f, -0.5f, 0.0f,
            -0.5f, -0.5f, 0.5f,
            -0.5f, -0.5f, -0.5f,

            0.0f, 0.5f, 0.0f,
            -0.5f, -0.5f, -0.5f,
            0.5f, -0.5f, -0.5f,

            0.0f, 0.5f, 0.0f,
            0.5f, -0.5f, -0.5f,
            0.5f, -0.5f, 0.5f,

            0.0f, 0.5f, 0.0f,
            0.5f, -0.5f, 0.5f,
            -0.5f, -0.5f, 0.5f,

            0.0f, 0.5f, 0.0f,
            -0.5f, -0.5f, 0.5f,
            -0.5f, -0.5f, -0.5f,
    };

    unsigned int pyramidVAO, pyramidVBO;
    glGenVertexArrays(1, &pyramidVAO);
    glGenBuffers(1, &pyramidVBO);
    glBindVertexArray(pyramidVAO);
    glBindBuffer(GL_ARRAY_BUFFER, pyramidVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(pyramidVertices), pyramidVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Terrain setup
    float terrainVertices[] = {
            // positions           // tex coords
            -500.0f, 0.0f, -500.0f, 0.0f, 30.0f,
            500.0f, 0.0f, -500.0f, 30.0f, 30.0f,
            500.0f, 0.0f, 500.0f, 30.0f, 0.0f,
            -500.0f, 0.0f, 500.0f, 0.0f, 0.0f
    };

    unsigned int terrainVBO, terrainVAO;
    glGenVertexArrays(1, &terrainVAO);
    glGenBuffers(1, &terrainVBO);
    glBindVertexArray(terrainVAO);
    glBindBuffer(GL_ARRAY_BUFFER, terrainVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(terrainVertices), terrainVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));

    unsigned int terrainBase = loadTexture("resources/textures/terrain/base.jpg");
    terrainShader.setInt("texture0", terrainBase);
    unsigned int terrainHeight = loadTexture("resources/textures/terrain/height.png");
    terrainShader.setInt("texture1", terrainHeight);
    unsigned int terrainRoughness = loadTexture("resources/textures/terrain/roughness.jpg");
    terrainShader.setInt("texture2", terrainRoughness);

    // Skybox setup
    float skyboxVertices[] = {
            // positions
            -1.0f, 1.0f, -1.0f,
            -1.0f, -1.0f, -1.0f,
            1.0f, -1.0f, -1.0f,
            1.0f, -1.0f, -1.0f,
            1.0f, 1.0f, -1.0f,
            -1.0f, 1.0f, -1.0f,

            -1.0f, -1.0f, 1.0f,
            -1.0f, -1.0f, -1.0f,
            -1.0f, 1.0f, -1.0f,
            -1.0f, 1.0f, -1.0f,
            -1.0f, 1.0f, 1.0f,
            -1.0f, -1.0f, 1.0f,

            1.0f, -1.0f, -1.0f,
            1.0f, -1.0f, 1.0f,
            1.0f, 1.0f, 1.0f,
            1.0f, 1.0f, 1.0f,
            1.0f, 1.0f, -1.0f,
            1.0f, -1.0f, -1.0f,

            -1.0f, -1.0f, 1.0f,
            -1.0f, 1.0f, 1.0f,
            1.0f, 1.0f, 1.0f,
            1.0f, 1.0f, 1.0f,
            1.0f, -1.0f, 1.0f,
            -1.0f, -1.0f, 1.0f,

            -1.0f, 1.0f, -1.0f,
            1.0f, 1.0f, -1.0f,
            1.0f, 1.0f, 1.0f,
            1.0f, 1.0f, 1.0f,
            -1.0f, 1.0f, 1.0f,
            -1.0f, 1.0f, -1.0f,

            -1.0f, -1.0f, -1.0f,
            -1.0f, -1.0f, 1.0f,
            1.0f, -1.0f, -1.0f,
            1.0f, -1.0f, -1.0f,
            -1.0f, -1.0f, 1.0f,
            1.0f, -1.0f, 1.0f
    };

    unsigned int skyboxVAO, skyboxVBO;
    glGenVertexArrays(1, &skyboxVAO);
    glGenBuffers(1, &skyboxVBO);
    glBindVertexArray(skyboxVAO);
    glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3*sizeof(float), (void*)0);

    vector<std::string> faces
            {
                    FileSystem::getPath("/resources/textures/skybox/right.png"),
                    FileSystem::getPath("/resources/textures/skybox/left.png"),
                    FileSystem::getPath("/resources/textures/skybox/top.png"),
                    FileSystem::getPath("/resources/textures/skybox/bottom.png"),
                    FileSystem::getPath("/resources/textures/skybox/front.png"),
                    FileSystem::getPath("/resources/textures/skybox/back.png")
            };
    unsigned int cubemapTexture = loadCubemap(faces);
    skyboxShader.use();
    skyboxShader.setInt("skybox", 0);

    // Directional light
    DirLight& dirLight = programState->dirLight;
    dirLight.direction = glm::vec3(-10.0, -10.0, -3.0);
    dirLight.ambient = glm::vec3(0.2, 0.2, 0.2);
    dirLight.diffuse = glm::vec3(0.1, 0.1, 0.1);
    dirLight.specular = glm::vec3(0.7, 0.7, 0.7);

    // Point light
    PointLight& pointLight = programState->ptLight;
    pointLight.position = programState->pyramidPosition;
    pointLight.specular = glm::vec3(1.0, 1.0, 1.0);
    pointLight.constant = 1.0f;
    pointLight.linear = 0.09f;
    pointLight.quadratic = 0.032f;

    // Spotlight
    SpotLight& spotLight = programState->spotLight;
    spotLight.ambient = glm::vec3(0.4, 0.4, 0.7);
    spotLight.diffuse = glm::vec3(0.5, 0.5, 0.9);
    spotLight.specular = glm::vec3(1.0, 1.0, 1.0);
    spotLight.constant = 1.0f;
    spotLight.linear = 0.09f;
    spotLight.quadratic = 0.032f;
    spotLight.cutOff = glm::cos(glm::radians(8.0f));
    spotLight.outerCutOff = glm::cos(glm::radians(12.0f));

    // Render loop
    while (!glfwWindowShouldClose(window)) {
        // Per-frame time logic
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // Input
        processInput(window);

        // Render
        glClearColor(programState->clearColor.r, programState->clearColor.g, programState->clearColor.b, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // View/projection transformations
        glm::mat4 projection = glm::perspective(glm::radians(programState->camera.Zoom), (float) SCR_WIDTH / (float) SCR_HEIGHT, 0.1f, 100.0f);
        glm::mat4 view = programState->camera.GetViewMatrix();
        glm::mat4 model = glm::mat4(1.0f);

        // Model lighting
        modelShader.use();
        modelShader.setFloat("material.shininess", 8.0f);
        modelShader.setBool("blinn", programState->blinn);

        // Directional light
        modelShader.setVec3("dirLight.direction", dirLight.direction);
        modelShader.setVec3("dirLight.ambient", dirLight.ambient);
        modelShader.setVec3("dirLight.diffuse", dirLight.diffuse);
        modelShader.setVec3("dirLight.specular", dirLight.specular);

        // Point light
        if(programState->randColor)
        {
            float red = (sin(currentFrame) + 1) / 2;
            float green = (cos(currentFrame * 1.5f) + 1) / 2;
            float blue = (sin(currentFrame * 0.5f) + 1) / 2;
            programState->pyramidColor = glm::vec3(red, green, blue);
        }
        modelShader.setVec3("ptLight.position", programState->pyramidPosition);
        modelShader.setVec3("ptLight.ambient", programState->pyramidColor * 0.1f);
        modelShader.setVec3("ptLight.diffuse", programState->pyramidColor);
        modelShader.setVec3("ptLight.specular", pointLight.specular);
        modelShader.setFloat("ptLight.constant", pointLight.constant);
        modelShader.setFloat("ptLight.linear", pointLight.linear);
        modelShader.setFloat("ptLight.quadratic", pointLight.quadratic);

        // Spotlight
        modelShader.setBool("spotLight.enabled", programState->spotLight.enabled);
        modelShader.setVec3("spotLight.position", programState->camera.Position);
        modelShader.setVec3("spotLight.direction", programState->camera.Front);
        modelShader.setVec3("spotLight.ambient", spotLight.ambient);
        modelShader.setVec3("spotLight.diffuse", spotLight.diffuse);
        modelShader.setVec3("spotLight.specular", spotLight.specular);
        modelShader.setFloat("spotLight.constant", spotLight.constant);
        modelShader.setFloat("spotLight.linear", spotLight.linear);
        modelShader.setFloat("spotLight.quadratic", spotLight.quadratic);
        modelShader.setFloat("spotLight.cutOff", spotLight.cutOff);
        modelShader.setFloat("spotLight.outerCutOff", spotLight.outerCutOff);

        // House render
        modelShader.setMat4("projection", projection);
        modelShader.setMat4("view", view);
        model = glm::mat4(1.0f);
        model = glm::translate(model, programState->housePosition);
        model = glm::scale(model, glm::vec3(programState->houseScale));
        modelShader.setMat4("model", model);
        house.Draw(modelShader);

        // Terrain render
        terrainShader.use();
        terrainShader.setInt("texture0", 0);
        terrainShader.setMat4("projection", projection);
        terrainShader.setMat4("view", view);
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(-50.0f, 0.0f, 0.0f));
        terrainShader.setMat4("model", model);
        glBindVertexArray(terrainVAO);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, terrainBase);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, terrainHeight);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, terrainRoughness);
        glDrawArrays(GL_TRIANGLES, 0, 4);
        glBindVertexArray(0);

        // Skybox render
        glDepthMask(GL_FALSE);
        glDepthFunc(GL_LEQUAL);
        skyboxShader.use();
        skyboxShader.setMat4("view", glm::mat4(glm::mat3(view)));
        skyboxShader.setMat4("projection", projection);
        glBindVertexArray(skyboxVAO);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);
        glDrawArrays(GL_TRIANGLES, 0, 36);
        glBindVertexArray(0);
        glDepthMask(GL_TRUE);
        glDepthFunc(GL_LESS);

        // Pyramid render
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_CONSTANT_ALPHA);
        glDepthMask(GL_FALSE);
        lightShader.use();
        lightShader.setVec3("color", programState->pyramidColor);
        lightShader.setMat4("projection", projection);
        lightShader.setMat4("view", view);
        glBindVertexArray(pyramidVAO);
        float angle = glfwGetTime() * glm::radians(70.0f);

        // Top pyramid
        model = glm::mat4(1.0f);
        model = glm::translate(model, programState->pyramidPosition);
        model = glm::rotate(model, angle, glm::vec3(0.0f, 1.0f, 0.0f));
        lightShader.setMat4("model", model);
        glDrawArrays(GL_TRIANGLES, 0, 24);

        // Bottom pyramid
        model = glm::mat4(1.0f);
        model = glm::translate(model, programState->pyramidPosition + glm::vec3(0.0f, -1.0f, 0.0f));
        model = glm::rotate(model, glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f));
        model = glm::rotate(model, -angle, glm::vec3(0.0f, 1.0f, 0.0f));
        lightShader.setMat4("model", model);
        glDrawArrays(GL_TRIANGLES, 0, 24);

        glBindVertexArray(0);
        glDisable(GL_BLEND);
        glDepthMask(GL_TRUE);

        // ImGui
        if (programState->ImGuiEnabled)
            DrawImGui(programState);

        // Swap buffers and poll IO events
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // De-allocate resources, save data and terminate GLFW
    programState->SaveToFile("resources/program_state.txt");
    delete programState;

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    modelShader.deleteProgram();
    lightShader.deleteProgram();
    terrainShader.deleteProgram();
    skyboxShader.deleteProgram();

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glDeleteVertexArrays(1, &pyramidVAO);
    glDeleteVertexArrays(1, &terrainVAO);
    glDeleteVertexArrays(1, &skyboxVAO);
    glDeleteBuffers(1, &pyramidVBO);
    glDeleteBuffers(1, &terrainVBO);
    glDeleteBuffers(1, &skyboxVBO);

    glfwTerminate();
    return 0;
}

// Process input
void processInput(GLFWwindow *window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        programState->camera.ProcessKeyboard(FORWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        programState->camera.ProcessKeyboard(BACKWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        programState->camera.ProcessKeyboard(LEFT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        programState->camera.ProcessKeyboard(RIGHT, deltaTime);
}

// Window size change
void framebuffer_size_callback(GLFWwindow *window, int width, int height) {
    // make sure the viewport matches the new window dimensions; note that width and
    // height will be significantly larger than specified on retina displays.
    glViewport(0, 0, width, height);
}

// Mouse movement
void mouse_callback(GLFWwindow *window, double xpos, double ypos) {
    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos; // reversed since y-coordinates go from bottom to top

    lastX = xpos;
    lastY = ypos;

    if (programState->CameraMouseMovementUpdateEnabled)
        programState->camera.ProcessMouseMovement(xoffset, yoffset);
}

// Mouse scroll
void scroll_callback(GLFWwindow *window, double xoffset, double yoffset) {
    programState->camera.ProcessMouseScroll(yoffset);
}

// ImGui
void DrawImGui(ProgramState *programState) {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    {
        ImGui::Begin("General info");
        const Camera& c = programState->camera;
        int min = (int)glfwGetTime() / 60;
        int sec = (int)glfwGetTime() - 60 * min;
        ImGui::Text("Program running time: %d minutes, %d seconds", min, sec);
        ImGui::Text("Camera position: (%f, %f, %f)", c.Position.x, c.Position.y, c.Position.z);
        ImGui::Text("Camera (yaw, pitch): (%f, %f)", c.Yaw, c.Pitch);
        ImGui::Checkbox("Camera mouse update", &programState->CameraMouseMovementUpdateEnabled);
        ImGui::End();
    }

    {
        ImGui::Begin("Light settings");
        ImGui::Text("Light position");
        {
            ImGui::SliderFloat("X", (float *) (&programState->pyramidPosition.x), 90.0f, 110.0f);
            ImGui::SliderFloat("Y", (float *) (&programState->pyramidPosition.y), 0.0f, 10.0f);
            ImGui::SliderFloat("Z", (float *) (&programState->pyramidPosition.z), -10.0f, 10.0f);
        }
        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::ColorEdit3("Light color", (float *) &programState->pyramidColor);
        ImGui::Checkbox("Random pyramid color", &programState->randColor);
        ImGui::Text("Lighting model: %s", programState->blinn ? "Blinn Phong" : "Phong");
        ImGui::End();
    }

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

// Keyboard
void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_L && action == GLFW_PRESS)
        programState->randColor = !programState->randColor;
    if (key == GLFW_KEY_SPACE && action == GLFW_PRESS)
        programState->blinn = !programState->blinn;
    if (key == GLFW_KEY_F && action == GLFW_PRESS)
        programState->spotLight.enabled = !programState->spotLight.enabled;
    if (key == GLFW_KEY_F1 && action == GLFW_PRESS) {
        programState->ImGuiEnabled = !programState->ImGuiEnabled;
        if (programState->ImGuiEnabled) {
            programState->CameraMouseMovementUpdateEnabled = false;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        } else {
            programState->CameraMouseMovementUpdateEnabled = true;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        }
    }
}

// 2D texture loading
unsigned int loadTexture(std::string pathToTex)
{
    unsigned int textureID;
    int width, height, nrChannels;
    unsigned char *data = stbi_load(pathToTex.c_str(), &width, &height, &nrChannels, 0);
    if (data)
    {
        glGenTextures(1, &textureID);
        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
    }
    else
    {
        std::cout << "Failed to load textures!" << std::endl;
    }

    return textureID;
}

// Cubemap loading
unsigned int loadCubemap(vector<std::string> faces)
{
    stbi_set_flip_vertically_on_load(false);
    unsigned int textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

    int width, height, nrChannels;
    unsigned char* data = nullptr;
    for (unsigned int i = 0; i < faces.size(); i++)
    {
        data = stbi_load(faces[i].c_str(), &width, &height, &nrChannels, 0);
        if (data)
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
        else
        {
            std::cerr << "Failed to load cubemap textures!" << std::endl;
            stbi_image_free(data);
            return -1;
        }
    }
    stbi_image_free(data);

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    stbi_set_flip_vertically_on_load(true);
    return textureID;
}