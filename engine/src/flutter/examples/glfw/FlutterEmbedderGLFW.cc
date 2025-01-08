// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cassert>
#include <chrono>
#include <iostream>

#include "GLFW/glfw3.h"
#include "embedder.h"

// This value is calculated after the window is created.
static double g_pixelRatio = 1.0;
static const size_t kInitialWindowWidth = 800;
static const size_t kInitialWindowHeight = 600;
static constexpr FlutterViewId kImplicitViewId = 0;

static_assert(FLUTTER_ENGINE_VERSION == 1,
              "This Flutter Embedder was authored against the stable Flutter "
              "API at version 1. There has been a serious breakage in the "
              "API. Please read the ChangeLog and take appropriate action "
              "before updating this assertion");

void GLFWcursorPositionCallbackAtPhase(GLFWwindow* window,
                                       FlutterPointerPhase phase,
                                       double x,
                                       double y) {
  FlutterPointerEvent event = {};
  event.struct_size = sizeof(event);
  event.phase = phase;
  event.x = x * g_pixelRatio;
  event.y = y * g_pixelRatio;
  event.timestamp =
      std::chrono::duration_cast<std::chrono::microseconds>(
          std::chrono::high_resolution_clock::now().time_since_epoch())
          .count();
  // This example only supports a single window, therefore we assume the pointer
  // event occurred in the only view, the implicit view.
  event.view_id = kImplicitViewId;
  FlutterEngineSendPointerEvent(
      reinterpret_cast<FlutterEngine>(glfwGetWindowUserPointer(window)), &event,
      1);
}

void GLFWcursorPositionCallback(GLFWwindow* window, double x, double y) {
  GLFWcursorPositionCallbackAtPhase(window, FlutterPointerPhase::kMove, x, y);
}

void GLFWmouseButtonCallback(GLFWwindow* window,
                             int key,
                             int action,
                             int mods) {
  if (key == GLFW_MOUSE_BUTTON_1 && action == GLFW_PRESS) {
    double x, y;
    glfwGetCursorPos(window, &x, &y);
    GLFWcursorPositionCallbackAtPhase(window, FlutterPointerPhase::kDown, x, y);
    glfwSetCursorPosCallback(window, GLFWcursorPositionCallback);
  }

  if (key == GLFW_MOUSE_BUTTON_1 && action == GLFW_RELEASE) {
    double x, y;
    glfwGetCursorPos(window, &x, &y);
    GLFWcursorPositionCallbackAtPhase(window, FlutterPointerPhase::kUp, x, y);
    glfwSetCursorPosCallback(window, nullptr);
  }
}

static void GLFWKeyCallback(GLFWwindow* window,
                            int key,
                            int scancode,
                            int action,
                            int mods) {
  if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
    glfwSetWindowShouldClose(window, GLFW_TRUE);
  }
}

void GLFWwindowSizeCallback(GLFWwindow* window, int width, int height) {
  FlutterWindowMetricsEvent event = {};
  event.struct_size = sizeof(event);
  event.width = width * g_pixelRatio;
  event.height = height * g_pixelRatio;
  event.pixel_ratio = g_pixelRatio;
  // This example only supports a single window, therefore we assume the event
  // occurred in the only view, the implicit view.
  event.view_id = kImplicitViewId;
  FlutterEngineSendWindowMetricsEvent(
      reinterpret_cast<FlutterEngine>(glfwGetWindowUserPointer(window)),
      &event);
}

bool RunFlutter(GLFWwindow* window,
                const std::string& project_path,
                const std::string& icudtl_path) {
  FlutterRendererConfig config = {};
  config.type = kOpenGL;
  config.open_gl.struct_size = sizeof(config.open_gl);
  config.open_gl.make_current = [](void* userdata) -> bool {
    glfwMakeContextCurrent(static_cast<GLFWwindow*>(userdata));
    return true;
  };
  config.open_gl.clear_current = [](void*) -> bool {
    glfwMakeContextCurrent(nullptr);  // is this even a thing?
    return true;
  };
  config.open_gl.present = [](void* userdata) -> bool {
    glfwSwapBuffers(static_cast<GLFWwindow*>(userdata));
    return true;
  };
  config.open_gl.fbo_callback = [](void*) -> uint32_t {
    return 0;  // FBO0
  };
  config.open_gl.gl_proc_resolver = [](void*, const char* name) -> void* {
    return reinterpret_cast<void*>(glfwGetProcAddress(name));
  };

  // This directory is generated by `flutter build bundle`.
  std::string assets_path = project_path + "/build/flutter_assets";
  FlutterProjectArgs args = {
      .struct_size = sizeof(FlutterProjectArgs),
      .assets_path = assets_path.c_str(),
      .icu_data_path =
          icudtl_path.c_str(),  // Find this in your bin/cache directory.
  };
  FlutterEngine engine = nullptr;
  FlutterEngineResult result =
      FlutterEngineRun(FLUTTER_ENGINE_VERSION, &config,  // renderer
                       &args, window, &engine);
  if (result != kSuccess || engine == nullptr) {
    std::cout << "Could not run the Flutter Engine." << std::endl;
    return false;
  }

  glfwSetWindowUserPointer(window, engine);
  GLFWwindowSizeCallback(window, kInitialWindowWidth, kInitialWindowHeight);

  return true;
}

void printUsage() {
  std::cout << "usage: embedder_example <path to project> <path to icudtl.dat>"
            << std::endl;
}

void GLFW_ErrorCallback(int error, const char* description) {
  std::cout << "GLFW Error: (" << error << ") " << description << std::endl;
}

int main(int argc, const char* argv[]) {
  if (argc != 3) {
    printUsage();
    return 1;
  }

  std::string project_path = argv[1];
  std::string icudtl_path = argv[2];

  glfwSetErrorCallback(GLFW_ErrorCallback);

  int result = glfwInit();
  if (result != GLFW_TRUE) {
    std::cout << "Could not initialize GLFW." << std::endl;
    return EXIT_FAILURE;
  }

#if defined(__linux__)
  glfwWindowHint(GLFW_CONTEXT_CREATION_API, GLFW_EGL_CONTEXT_API);
#endif

  GLFWwindow* window = glfwCreateWindow(
      kInitialWindowWidth, kInitialWindowHeight, "Flutter", NULL, NULL);
  if (window == nullptr) {
    std::cout << "Could not create GLFW window." << std::endl;
    return EXIT_FAILURE;
  }

  int framebuffer_width, framebuffer_height;
  glfwGetFramebufferSize(window, &framebuffer_width, &framebuffer_height);
  g_pixelRatio = framebuffer_width / kInitialWindowWidth;

  bool run_result = RunFlutter(window, project_path, icudtl_path);
  if (!run_result) {
    std::cout << "Could not run the Flutter engine." << std::endl;
    return EXIT_FAILURE;
  }

  glfwSetKeyCallback(window, GLFWKeyCallback);
  glfwSetWindowSizeCallback(window, GLFWwindowSizeCallback);
  glfwSetMouseButtonCallback(window, GLFWmouseButtonCallback);

  while (!glfwWindowShouldClose(window)) {
    glfwWaitEvents();
  }

  glfwDestroyWindow(window);
  glfwTerminate();

  return EXIT_SUCCESS;
}