/*
 *  Copyright (c) 2009-2011, NVIDIA Corporation
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *      * Redistributions of source code must retain the above copyright
 *        notice, this list of conditions and the following disclaimer.
 *      * Redistributions in binary form must reproduce the above copyright
 *        notice, this list of conditions and the following disclaimer in the
 *        documentation and/or other materials provided with the distribution.
 *      * Neither the name of NVIDIA Corporation nor the
 *        names of its contributors may be used to endorse or promote products
 *        derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 *  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 *  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *  DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
 *  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 *  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 *  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 *  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// Adapted for Aalto CS-C3100 Computer Graphics from source code with the copyright above.

#include "glad/gl_core_33.h"

#include <iostream>
#include <vector>
#include <fmt/core.h>

#include <Eigen/Dense>              // Linear algebra
#include <Eigen/Geometry>

using namespace std;
using namespace Eigen;

#include "Utils.h"
#include "ShaderProgram.h"


//------------------------------------------------------------------------

ShaderProgram::ShaderProgram(const string& vertexSource, const string& fragmentSource)
{
    init(vertexSource, 0, 0, 0, "", fragmentSource);
}

//------------------------------------------------------------------------

ShaderProgram::ShaderProgram(
    const string& vertexSource,
    GLenum geomInputType, GLenum geomOutputType, int geomVerticesOut, const string& geometrySource,
    const string& fragmentSource)
{
    init(vertexSource, geomInputType, geomOutputType, geomVerticesOut, geometrySource, fragmentSource);
}

//------------------------------------------------------------------------

ShaderProgram::~ShaderProgram(void)
{
    glDeleteProgram(m_glProgram);
    glDeleteShader(m_glVertexShader);
    glDeleteShader(m_glGeometryShader);
    glDeleteShader(m_glFragmentShader);
}

//------------------------------------------------------------------------

GLint ShaderProgram::getAttribLoc(const string& name) const
{
    return glGetAttribLocation(m_glProgram, name.c_str());
}

//------------------------------------------------------------------------

GLint ShaderProgram::getUniformLoc(const string& name) const
{
    return glGetUniformLocation(m_glProgram, name.c_str());
}

//------------------------------------------------------------------------

void ShaderProgram::use(void)
{
    glUseProgram(m_glProgram);
}

//------------------------------------------------------------------------

GLuint ShaderProgram::createGLShader(GLenum type, const string& typeStr, const string& source)
{
    GLuint shader = glCreateShader(type);
    const char* sourcePtr = source.c_str();
    int sourceLen = source.size();
    glShaderSource(shader, 1, &sourcePtr, &sourceLen);
    glCompileShader(shader);

    GLint status = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (!status)
    {
        GLint infoLen = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);
        if (!infoLen)
            fail("glGetShaderiv() failed!");

        string info;
        info.resize(infoLen);
        glGetShaderInfoLog(shader, infoLen, &infoLen, info.data());
        std::cout << info << std::endl;
        throw(ShaderCompilationException(fmt::format("glCompileShader({}) failed!\n\n{}", typeStr, info)));
    }

    // checkErrors();
    return shader;
}

//------------------------------------------------------------------------

void ShaderProgram::linkGLProgram(GLuint prog)
{
    glLinkProgram(prog);
    GLint status = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &status);
    if (!status)
    {
        GLint infoLen = 0;
        glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &infoLen);
        if (!infoLen)
            fail("glGetProgramiv() failed!");
        
        string info;
        info.resize(infoLen);
        glGetProgramInfoLog(prog, infoLen, &infoLen, info.data());
        throw(ShaderCompilationException(fmt::format("glLinkProgram() failed!\n\n{}", info)));
    }
    //checkErrors();
}

//------------------------------------------------------------------------

void ShaderProgram::init(
    const string& vertexSource,
    GLenum geomInputType, GLenum geomOutputType, int geomVerticesOut, const string& geometrySource,
    const string& fragmentSource)
{
    //staticInit();
    m_glProgram = glCreateProgram();

    // Setup vertex shader.

    m_glVertexShader = createGLShader(GL_VERTEX_SHADER, "GL_VERTEX_SHADER", vertexSource);
    glAttachShader(m_glProgram, m_glVertexShader);

    // Setup geometry shader (GL_ARB_geometry_shader4).

    if (geometrySource.size() == 0)
        m_glGeometryShader = 0;
    else
    {
        m_glGeometryShader = createGLShader(GL_GEOMETRY_SHADER, "GL_GEOMETRY_SHADER", geometrySource);
        glAttachShader(m_glProgram, m_glGeometryShader);

        if(glProgramParameteri == 0)
            fail("glProgramParameteri() not available!");
        glProgramParameteri(m_glProgram, GL_GEOMETRY_INPUT_TYPE, geomInputType);
        glProgramParameteri(m_glProgram, GL_GEOMETRY_OUTPUT_TYPE, geomOutputType);
        glProgramParameteri(m_glProgram, GL_GEOMETRY_VERTICES_OUT, geomVerticesOut);
    }

    // Setup fragment shader.

    m_glFragmentShader = createGLShader(GL_FRAGMENT_SHADER, "GL_FRAGMENT_SHADER", fragmentSource);
    glAttachShader(m_glProgram, m_glFragmentShader);

    // Link.

    linkGLProgram(m_glProgram);
}
