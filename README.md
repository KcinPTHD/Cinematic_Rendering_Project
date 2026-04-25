** Compilar

g++ -g src/*.cpp C:/OpenGL/glad/src/glad.c -o bin/app.exe -I C:/OpenGL/glfw/include -I C:/OpenGL/glm/include -I C:/OpenGL/glad/include -L C:/OpenGL/glfw/lib-mingw-w64 -lglfw3dll -lopengl32

** Executar

bin/app.exe
