//
// Copyright (c) 2002-2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "GLSLANG/ShaderLang.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include "angle_gl.h"

//
// Return codes from main.
//
enum TFailCode
{
    ESuccess = 0,
    EFailUsage,
    EFailCompile,
    EFailCompilerCreate,
};

static void usage();
static sh::GLenum FindShaderType(const char *fileName);
static bool CompileFile(char *fileName, ShHandle compiler, int compileOptions);
static void LogMsg(const char *msg, const char *name, const int num, const char *logName);
static void PrintVariable(const std::string &prefix, size_t index, const sh::ShaderVariable &var);
static void PrintActiveVariables(ShHandle compiler);

// If NUM_SOURCE_STRINGS is set to a value > 1, the input file data is
// broken into that many chunks.
const unsigned int NUM_SOURCE_STRINGS = 2;
typedef std::vector<char *> ShaderSource;
static bool ReadShaderSource(const char *fileName, ShaderSource &source);
static void FreeShaderSource(ShaderSource &source);

//
// Set up the per compile resources
//
void GenerateResources(ShBuiltInResources *resources)
{
    ShInitBuiltInResources(resources);

    resources->MaxVertexAttribs = 8;
    resources->MaxVertexUniformVectors = 128;
    resources->MaxVaryingVectors = 8;
    resources->MaxVertexTextureImageUnits = 0;
    resources->MaxCombinedTextureImageUnits = 8;
    resources->MaxTextureImageUnits = 8;
    resources->MaxFragmentUniformVectors = 16;
    resources->MaxDrawBuffers = 1;

    resources->OES_standard_derivatives = 0;
    resources->OES_EGL_image_external = 0;
}

int main(int argc, char *argv[])
{
    TFailCode failCode = ESuccess;

    int compileOptions = 0;
    int numCompiles = 0;
    ShHandle vertexCompiler = 0;
    ShHandle fragmentCompiler = 0;
    ShShaderSpec spec = SH_GLES2_SPEC;
    ShShaderOutput output = SH_ESSL_OUTPUT;

    ShInitialize();

    ShBuiltInResources resources;
    GenerateResources(&resources);

    argc--;
    argv++;
    for (; (argc >= 1) && (failCode == ESuccess); argc--, argv++)
    {
        if (argv[0][0] == '-')
        {
            switch (argv[0][1])
            {
              case 'i': compileOptions |= SH_INTERMEDIATE_TREE; break;
              case 'o': compileOptions |= SH_OBJECT_CODE; break;
              case 'u': compileOptions |= SH_VARIABLES; break;
              case 'l': compileOptions |= SH_UNROLL_FOR_LOOP_WITH_INTEGER_INDEX; break;
              case 'e': compileOptions |= SH_EMULATE_BUILT_IN_FUNCTIONS; break;
              case 'd': compileOptions |= SH_DEPENDENCY_GRAPH; break;
              case 't': compileOptions |= SH_TIMING_RESTRICTIONS; break;
              case 's':
                if (argv[0][2] == '=')
                {
                    switch (argv[0][3])
                    {
                      case 'e': spec = SH_GLES2_SPEC; break;
                      case 'w': spec = SH_WEBGL_SPEC; break;
                      case 'c': spec = SH_CSS_SHADERS_SPEC; break;
                      default: failCode = EFailUsage;
                    }
                }
                else
                {
                    failCode = EFailUsage;
                }
                break;
              case 'b':
                if (argv[0][2] == '=')
                {
                    switch (argv[0][3])
                    {
                      case 'e': output = SH_ESSL_OUTPUT; break;
                      case 'g': output = SH_GLSL_OUTPUT; break;
                      case 'h':
                        if (argv[0][4] == '1' && argv[0][5] == '1')
                        {
                            output = SH_HLSL11_OUTPUT;
                        }
                        else
                        {
                            output = SH_HLSL9_OUTPUT;
                        }
                        break;
                      default: failCode = EFailUsage;
                    }
                }
                else
                {
                    failCode = EFailUsage;
                }
                break;
              case 'x':
                if (argv[0][2] == '=')
                {
                    switch (argv[0][3])
                    {
                      case 'i': resources.OES_EGL_image_external = 1; break;
                      case 'd': resources.OES_standard_derivatives = 1; break;
                      case 'r': resources.ARB_texture_rectangle = 1; break;
                      case 'l': resources.EXT_shader_texture_lod = 1; break;
                      case 'f': resources.EXT_shader_framebuffer_fetch = 1; break;
                      case 'n': resources.NV_shader_framebuffer_fetch = 1; break;
                      case 'a': resources.ARM_shader_framebuffer_fetch = 1; break;
                      default: failCode = EFailUsage;
                    }
                }
                else
                {
                    failCode = EFailUsage;
                }
                break;
              default: failCode = EFailUsage;
            }
        }
        else
        {
            ShHandle compiler = 0;
            switch (FindShaderType(argv[0]))
            {
              case GL_VERTEX_SHADER:
                if (vertexCompiler == 0)
                {
                    vertexCompiler = ShConstructCompiler(
                        GL_VERTEX_SHADER, spec, output, &resources);
                }
                compiler = vertexCompiler;
                break;
              case GL_FRAGMENT_SHADER:
                if (fragmentCompiler == 0)
                {
                    fragmentCompiler = ShConstructCompiler(
                        GL_FRAGMENT_SHADER, spec, output, &resources);
                }
                compiler = fragmentCompiler;
                break;
              default: break;
            }
            if (compiler)
            {
                bool compiled = CompileFile(argv[0], compiler, compileOptions);

                LogMsg("BEGIN", "COMPILER", numCompiles, "INFO LOG");
                std::string log = ShGetInfoLog(compiler);
                puts(log.c_str());
                LogMsg("END", "COMPILER", numCompiles, "INFO LOG");
                printf("\n\n");

                if (compiled && (compileOptions & SH_OBJECT_CODE))
                {
                    LogMsg("BEGIN", "COMPILER", numCompiles, "OBJ CODE");
                    std::string code = ShGetObjectCode(compiler);
                    puts(code.c_str());
                    LogMsg("END", "COMPILER", numCompiles, "OBJ CODE");
                    printf("\n\n");
                }
                if (compiled && (compileOptions & SH_VARIABLES))
                {
                    LogMsg("BEGIN", "COMPILER", numCompiles, "VARIABLES");
                    PrintActiveVariables(compiler);
                    LogMsg("END", "COMPILER", numCompiles, "VARIABLES");
                    printf("\n\n");
                }
                if (!compiled)
                  failCode = EFailCompile;
                ++numCompiles;
            }
            else
            {
                failCode = EFailCompilerCreate;
            }
        }
    }

    if ((vertexCompiler == 0) && (fragmentCompiler == 0))
        failCode = EFailUsage;
    if (failCode == EFailUsage)
        usage();

    if (vertexCompiler)
        ShDestruct(vertexCompiler);
    if (fragmentCompiler)
        ShDestruct(fragmentCompiler);
    ShFinalize();

    return failCode;
}

//
//   print usage to stdout
//
void usage()
{
    printf("Usage: translate [-i -m -o -u -l -e -b=e -b=g -b=h -x=i -x=d] file1 file2 ...\n"
        "Where: filename : filename ending in .frag or .vert\n"
        "       -i       : print intermediate tree\n"
        "       -o       : print translated code\n"
        "       -u       : print active attribs, uniforms, and varyings\n"
        "       -l       : unroll for-loops with integer indices\n"
        "       -e       : emulate certain built-in functions (workaround for driver bugs)\n"
        "       -t       : enforce experimental timing restrictions\n"
        "       -d       : print dependency graph used to enforce timing restrictions\n"
        "       -s=e     : use GLES2 spec (this is by default)\n"
        "       -s=w     : use WebGL spec\n"
        "       -s=c     : use CSS Shaders spec\n"
        "       -b=e     : output GLSL ES code (this is by default)\n"
        "       -b=g     : output GLSL code\n"
        "       -b=h9    : output HLSL9 code\n"
        "       -b=h11   : output HLSL11 code\n"
        "       -x=i     : enable GL_OES_EGL_image_external\n"
        "       -x=d     : enable GL_OES_EGL_standard_derivatives\n"
        "       -x=r     : enable ARB_texture_rectangle\n"
        "       -x=l     : enable EXT_shader_texture_lod\n"
        "       -x=f     : enable EXT_shader_framebuffer_fetch\n"
        "       -x=n     : enable NV_shader_framebuffer_fetch\n"
        "       -x=a     : enable ARM_shader_framebuffer_fetch\n");
}

//
//   Deduce the shader type from the filename.  Files must end in one of the
//   following extensions:
//
//   .frag*    = fragment shader
//   .vert*    = vertex shader
//
sh::GLenum FindShaderType(const char *fileName)
{
    assert(fileName);

    const char *ext = strrchr(fileName, '.');

    if (ext && strcmp(ext, ".sl") == 0)
        for (; ext > fileName && ext[0] != '.'; ext--);

    ext = strrchr(fileName, '.');
    if (ext)
    {
        if (strncmp(ext, ".frag", 4) == 0) return GL_FRAGMENT_SHADER;
        if (strncmp(ext, ".vert", 4) == 0) return GL_VERTEX_SHADER;
    }

    return GL_FRAGMENT_SHADER;
}

//
//   Read a file's data into a string, and compile it using ShCompile
//
bool CompileFile(char *fileName, ShHandle compiler, int compileOptions)
{
    ShaderSource source;
    if (!ReadShaderSource(fileName, source))
        return false;

    int ret = ShCompile(compiler, &source[0], source.size(), compileOptions);

    FreeShaderSource(source);
    return ret ? true : false;
}

void LogMsg(const char *msg, const char *name, const int num, const char *logName)
{
    printf("#### %s %s %d %s ####\n", msg, name, num, logName);
}

void PrintVariable(const std::string &prefix, size_t index, const sh::ShaderVariable &var)
{
    std::string typeName;
    switch (var.type)
    {
      case GL_FLOAT: typeName = "GL_FLOAT"; break;
      case GL_FLOAT_VEC2: typeName = "GL_FLOAT_VEC2"; break;
      case GL_FLOAT_VEC3: typeName = "GL_FLOAT_VEC3"; break;
      case GL_FLOAT_VEC4: typeName = "GL_FLOAT_VEC4"; break;
      case GL_INT: typeName = "GL_INT"; break;
      case GL_INT_VEC2: typeName = "GL_INT_VEC2"; break;
      case GL_INT_VEC3: typeName = "GL_INT_VEC3"; break;
      case GL_INT_VEC4: typeName = "GL_INT_VEC4"; break;
      case GL_UNSIGNED_INT: typeName = "GL_UNSIGNED_INT"; break;
      case GL_UNSIGNED_INT_VEC2: typeName = "GL_UNSIGNED_INT_VEC2"; break;
      case GL_UNSIGNED_INT_VEC3: typeName = "GL_UNSIGNED_INT_VEC3"; break;
      case GL_UNSIGNED_INT_VEC4: typeName = "GL_UNSIGNED_INT_VEC4"; break;
      case GL_BOOL: typeName = "GL_BOOL"; break;
      case GL_BOOL_VEC2: typeName = "GL_BOOL_VEC2"; break;
      case GL_BOOL_VEC3: typeName = "GL_BOOL_VEC3"; break;
      case GL_BOOL_VEC4: typeName = "GL_BOOL_VEC4"; break;
      case GL_FLOAT_MAT2: typeName = "GL_FLOAT_MAT2"; break;
      case GL_FLOAT_MAT3: typeName = "GL_FLOAT_MAT3"; break;
      case GL_FLOAT_MAT4: typeName = "GL_FLOAT_MAT4"; break;
      case GL_FLOAT_MAT2x3: typeName = "GL_FLOAT_MAT2x3"; break;
      case GL_FLOAT_MAT3x2: typeName = "GL_FLOAT_MAT3x2"; break;
      case GL_FLOAT_MAT4x2: typeName = "GL_FLOAT_MAT4x2"; break;
      case GL_FLOAT_MAT2x4: typeName = "GL_FLOAT_MAT2x4"; break;
      case GL_FLOAT_MAT3x4: typeName = "GL_FLOAT_MAT3x4"; break;
      case GL_FLOAT_MAT4x3: typeName = "GL_FLOAT_MAT4x3"; break;
      case GL_SAMPLER_2D: typeName = "GL_SAMPLER_2D"; break;
      case GL_SAMPLER_CUBE: typeName = "GL_SAMPLER_CUBE"; break;
      case GL_SAMPLER_EXTERNAL_OES: typeName = "GL_SAMPLER_EXTERNAL_OES"; break;
      default: typeName = "UNKNOWN"; break;
    }

    printf("%s %lu : name=%s, type=%s, arraySize=%u\n",
           prefix.c_str(), index, var.name.c_str(), typeName.c_str(), var.arraySize);
    if (var.fields.size())
    {
        std::string structPrefix;
        for (size_t i = 0; i < prefix.size(); ++i)
            structPrefix += ' ';
        printf("%s  struct %s\n", structPrefix.c_str(), var.structName.c_str());
        structPrefix += "    field";
        for (size_t i = 0; i < var.fields.size(); ++i)
            PrintVariable(structPrefix, i, var.fields[i]);
    }
}

static void PrintActiveVariables(ShHandle compiler)
{
    const std::vector<sh::Uniform> *uniforms = ShGetUniforms(compiler);
    const std::vector<sh::Varying> *varyings = ShGetVaryings(compiler);
    const std::vector<sh::Attribute> *attributes = ShGetAttributes(compiler);
    for (size_t varCategory = 0; varCategory < 3; ++varCategory)
    {
        size_t numVars = 0;
        std::string varCategoryName;
        if (varCategory == 0)
        {
            numVars = uniforms->size();
            varCategoryName = "uniform";
        }
        else if (varCategory == 1)
        {
            numVars = varyings->size();
            varCategoryName = "varying";
        }
        else
        {
            numVars = attributes->size();
            varCategoryName = "attribute";
        }
        for (size_t i = 0; i < numVars; ++i)
        {
            const sh::ShaderVariable *var;
            if (varCategory == 0)
                var = &((*uniforms)[i]);
            else if (varCategory == 1)
                var = &((*varyings)[i]);
            else
                var = &((*attributes)[i]);
            PrintVariable(varCategoryName, i, *var);
        }
        printf("\n");
    }
}

static bool ReadShaderSource(const char *fileName, ShaderSource &source)
{
    FILE *in = fopen(fileName, "rb");
    if (!in)
    {
        printf("Error: unable to open input file: %s\n", fileName);
        return false;
    }

    // Obtain file size.
    fseek(in, 0, SEEK_END);
    size_t count = ftell(in);
    rewind(in);

    int len = (int)ceil((float)count / (float)NUM_SOURCE_STRINGS);
    source.reserve(NUM_SOURCE_STRINGS);
    // Notice the usage of do-while instead of a while loop here.
    // It is there to handle empty files in which case a single empty
    // string is added to vector.
    do
    {
        char *data = new char[len + 1];
        size_t nread = fread(data, 1, len, in);
        data[nread] = '\0';
        source.push_back(data);

        count -= nread;
    }
    while (count > 0);

    fclose(in);
    return true;
}

static void FreeShaderSource(ShaderSource &source)
{
    for (ShaderSource::size_type i = 0; i < source.size(); ++i)
    {
        delete [] source[i];
    }
    source.clear();
}

