/*     Copyright 2015-2018 Egor Yusov
 *  
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF ANY PROPRIETARY RIGHTS.
 *
 *  In no event and under no legal theory, whether in tort (including negligence), 
 *  contract, or otherwise, unless required by applicable law (such as deliberate 
 *  and grossly negligent acts) or agreed to in writing, shall any Contributor be
 *  liable for any damages, including any direct, indirect, special, incidental, 
 *  or consequential damages of any character arising as a result of this License or 
 *  out of the use or inability to use the software (including but not limited to damages 
 *  for loss of goodwill, work stoppage, computer failure or malfunction, or any and 
 *  all other commercial damages or losses), even if such Contributor has been advised 
 *  of the possibility of such damages.
 */

#include "pch.h"
#include "BufferParser.h"

namespace Diligent
{
    const Char* BufferParser::BufferLibName = "Buffer";

    BufferParser::BufferParser( IRenderDevice *pRenderDevice, lua_State *L ) :
        EngineObjectParserCommon<IBuffer>( pRenderDevice, L, BufferLibName ),
        m_SetVertexBuffersBinding(this, L, "Context", "SetVertexBuffers", &BufferParser::SetVertexBuffers),
        m_SetIndexBufferBinding( this, L, "Context", "SetIndexBuffer", &BufferParser::SetIndexBuffer )
    {
        DEFINE_BUFFERED_STRING_BINDER( m_Bindings, SBuffDescWrapper, Name, NameBuffer );

        DEFINE_BINDER( m_Bindings, SBuffDescWrapper, uiSizeInBytes );

        DEFINE_ENUM_ELEMENT_MAPPING( m_BindFlagEnumMapping, BIND_VERTEX_BUFFER );
        DEFINE_ENUM_ELEMENT_MAPPING( m_BindFlagEnumMapping, BIND_INDEX_BUFFER );
        DEFINE_ENUM_ELEMENT_MAPPING( m_BindFlagEnumMapping, BIND_UNIFORM_BUFFER );
        DEFINE_ENUM_ELEMENT_MAPPING( m_BindFlagEnumMapping, BIND_SHADER_RESOURCE );
        DEFINE_ENUM_ELEMENT_MAPPING( m_BindFlagEnumMapping, BIND_STREAM_OUTPUT );
        //DEFINE_ENUM_ELEMENT_MAPPING( m_BindFlagEnumMapping, BIND_RENDER_TARGET );
        //DEFINE_ENUM_ELEMENT_MAPPING( m_BindFlagEnumMapping, BIND_DEPTH_STENCIL );
        DEFINE_ENUM_ELEMENT_MAPPING( m_BindFlagEnumMapping, BIND_UNORDERED_ACCESS );
        DEFINE_ENUM_ELEMENT_MAPPING( m_BindFlagEnumMapping, BIND_INDIRECT_DRAW_ARGS );
        // Explicit namespace declaraion is necesseary to avoid 
        // name conflicts when building for windows store
        DEFINE_FLAGS_BINDER( m_Bindings, SBuffDescWrapper, BindFlags, Diligent::BIND_FLAGS, m_BindFlagEnumMapping );

        DEFINE_ENUM_BINDER( m_Bindings, SBuffDescWrapper, Usage, m_UsageEnumMapping );
        DEFINE_FLAGS_BINDER( m_Bindings, SBuffDescWrapper, CPUAccessFlags, CPU_ACCESS_FLAGS, m_CpuAccessFlagEnumMapping );
        
        DEFINE_ENUM_ELEMENT_MAPPING( m_BuffModeEnumMapping, BUFFER_MODE_UNDEFINED );
        DEFINE_ENUM_ELEMENT_MAPPING( m_BuffModeEnumMapping, BUFFER_MODE_FORMATTED );
        DEFINE_ENUM_ELEMENT_MAPPING( m_BuffModeEnumMapping, BUFFER_MODE_STRUCTURED );
        DEFINE_ENUM_ELEMENT_MAPPING( m_BuffModeEnumMapping, BUFFER_MODE_RAW );
        static_assert(BUFFER_MODE_NUM_MODES == BUFFER_MODE_RAW + 1, "Not all buffer modes initialized.");
        VERIFY( m_BuffModeEnumMapping.m_Str2ValMap.size() == BUFFER_MODE_NUM_MODES,
                "Unexpected map size. Did you update BUFFER_MODE enum?" );
        VERIFY( m_BuffModeEnumMapping.m_Val2StrMap.size() == BUFFER_MODE_NUM_MODES,
                "Unexpected map size. Did you update BUFFER_MODE enum?" );
        DEFINE_ENUM_BINDER( m_Bindings, SBuffDescWrapper, Mode, m_BuffModeEnumMapping );

        DEFINE_BINDER( m_Bindings, SBuffDescWrapper, ElementByteStride );

        DEFINE_ENUM_ELEMENT_MAPPING( m_SetVBFlagEnumMapping, SET_VERTEX_BUFFERS_FLAG_NONE );
        DEFINE_ENUM_ELEMENT_MAPPING( m_SetVBFlagEnumMapping, SET_VERTEX_BUFFERS_FLAG_RESET );
    };

    void BufferParser::CreateObj( lua_State *L )
    {
        auto NumArgs = lua_gettop( L );
        INIT_LUA_STACK_TRACKING(L);
        SBuffDescWrapper BufferDesc;
        ParseLuaTable( L, 1, &BufferDesc, m_Bindings );
        CHECK_LUA_STACK_HEIGHT();
        
        if( (BufferDesc.Mode == BUFFER_MODE_STRUCTURED || BufferDesc.Mode == BUFFER_MODE_FORMATTED) && BufferDesc.ElementByteStride == 0 )
            SCRIPT_PARSING_ERROR( L, "Element byte stride of a structured or formatted buffer cannot be zero" );

        if( (BufferDesc.Mode == BUFFER_MODE_FORMATTED || BufferDesc.Mode == BUFFER_MODE_STRUCTURED) &&
            (BufferDesc.uiSizeInBytes % BufferDesc.ElementByteStride) != 0 )
            SCRIPT_PARSING_ERROR( L, "Buffer size (", BufferDesc.uiSizeInBytes, ") is not multiple of element byte stride (", BufferDesc.ElementByteStride, ")."  );

        std::vector<Uint8> RawData;
        if( NumArgs > 1 )
        {
            if( NumArgs != 3 )
            {
                SCRIPT_PARSING_ERROR( L, "To initialize buffer with initial data, provide value type and array of values as the 2nd and 3rd parameters. ", NumArgs, " arguments is provided." );
            }

            m_ArrayLoader.LoadArray( L, 3, RawData );
        }
        BufferData BuffData;
        auto DataSize = static_cast<Uint32>(RawData.size());
        if( DataSize )
        {
            if( BufferDesc.uiSizeInBytes == 0 )
                BufferDesc.uiSizeInBytes = DataSize;
            if( DataSize != BufferDesc.uiSizeInBytes )
            {
                SCRIPT_PARSING_ERROR( L, "Initial buffer data size (", DataSize, ") does not match the requested buffer size (", BufferDesc.uiSizeInBytes, "). "
                                         "Do not specify uiSizeInBytes to have the buffer size calculated automatically." );
            }
            BuffData.pData = RawData.data();
            BuffData.DataSize = DataSize;
        }

        if( (BufferDesc.BindFlags & BIND_UNIFORM_BUFFER) && (BufferDesc.uiSizeInBytes % 16) )
        { 
            SCRIPT_PARSING_ERROR( L, "Uniform buffer size (", DataSize, ") is not multiple of 16." );
        }

        auto ppBuffer = reinterpret_cast<IBuffer**>(lua_newuserdata( L, sizeof( IBuffer* ) ));
        *ppBuffer = nullptr;
        m_pRenderDevice->CreateBuffer( BufferDesc, BuffData, ppBuffer );
        if( *ppBuffer == nullptr )
            SCRIPT_PARSING_ERROR( L, "Failed to create buffer" )

        CHECK_LUA_STACK_HEIGHT( +1 );
    }

    int BufferParser::SetVertexBuffers( lua_State *L )
    {
        auto NumArgs = lua_gettop( L );
        
        int CurrArgInd = 1;
        
        Int32 StartSlot = 0;
        if( lua_type( L, CurrArgInd ) == LUA_TNUMBER )
        {
            StartSlot = ReadValueFromLua<Int32>( L, CurrArgInd++ );
            if( StartSlot < 0 )
            {
                SCRIPT_PARSING_ERROR( L, "Start slot (", StartSlot, " provided) must be in range 0..", MaxBufferSlots - 1 );
            }
        }

        SET_VERTEX_BUFFERS_FLAGS Flags = SET_VERTEX_BUFFERS_FLAG_NONE;
        RESOURCE_STATE_TRANSITION_MODE StateTransitionMode = RESOURCE_STATE_TRANSITION_MODE_NONE;
        Uint32 NumBuffers = 0;
        IBuffer *pBuffs[MaxBufferSlots] = {};
        Uint32 Offsets[MaxBufferSlots] = {};
        while( CurrArgInd <= NumArgs )
        {
            if( StartSlot + (NumBuffers + 1) > MaxBufferSlots )
            {
                SCRIPT_PARSING_ERROR( L, "Too many buffer slots (", StartSlot, "..", StartSlot + NumBuffers - 1, ") are being set. Allowed slots are 0..", MaxBufferSlots - 1 );
                break;
            }

            if( lua_type( L, CurrArgInd ) != LUA_TNIL )
                pBuffs[NumBuffers] = *GetUserData<IBuffer**>( L, CurrArgInd++, m_MetatableRegistryName.c_str() );
            else
            {
                ++CurrArgInd;
                pBuffs[NumBuffers] = nullptr;
            }

            if( lua_type( L, CurrArgInd ) == LUA_TNUMBER )
                Offsets[NumBuffers] = ReadValueFromLua<Uint32>( L, CurrArgInd++ );
            else
                Offsets[NumBuffers] = 0;

            if( CurrArgInd >= NumArgs-1 && lua_type( L, CurrArgInd ) == LUA_TSTRING)
            {
                EnumMemberBinder<RESOURCE_STATE_TRANSITION_MODE> StateTransitionModeLoader(0, "StateTransitionMode", m_StateTransitionModeMapping);
                StateTransitionModeLoader.SetValue( L, CurrArgInd, &StateTransitionMode );
                ++CurrArgInd;
            }

            // The last argument may be flags
            if( CurrArgInd == NumArgs &&
                (lua_type( L, CurrArgInd ) == LUA_TSTRING || 
                 lua_type( L, CurrArgInd ) == LUA_TTABLE ) )
            {
                VERIFY( Flags == 0, "Flags have already been set!" );
                FlagsLoader<SET_VERTEX_BUFFERS_FLAGS> SetVBFlagsLoader(0, "SetVBFlags", m_SetVBFlagEnumMapping);
                SetVBFlagsLoader.SetValue( L, CurrArgInd, &Flags );
                ++CurrArgInd;
            }

            ++NumBuffers;
        }

        auto *pContext = LoadDeviceContextFromRegistry( L );
        pContext->SetVertexBuffers( StartSlot, NumBuffers, pBuffs, Offsets, StateTransitionMode, Flags );
        
        // Return no values to Lua
        return 0;
    }

    int BufferParser::SetIndexBuffer( lua_State *L )
    {
        auto *pIndexBuff = *GetUserData<IBuffer**>( L, 1, m_MetatableRegistryName.c_str() );
        Uint32 Offset = 0;
        auto NumArgs = lua_gettop( L );
        if( NumArgs > 3 )
        {
            SCRIPT_PARSING_ERROR( L, "SetIndexBuffer() expects offset and state transition mode as optional 2nd and 3rd parameters. ", NumArgs, " arguments are provided." );
        }
        int CurrArg = 2;
        if( CurrArg <= NumArgs && lua_isnumber( L, CurrArg ) )
        {
            Offset = ReadValueFromLua<Uint32>( L, CurrArg );
            ++CurrArg;
        }

        RESOURCE_STATE_TRANSITION_MODE StateTransitionMode = RESOURCE_STATE_TRANSITION_MODE_NONE;
        if( CurrArg <= NumArgs && lua_type( L, CurrArg ) == LUA_TSTRING)
        {
            EnumMemberBinder<RESOURCE_STATE_TRANSITION_MODE> StateTransitionModeLoader(0, "StateTransitionMode", m_StateTransitionModeMapping);
            StateTransitionModeLoader.SetValue( L, CurrArg, &StateTransitionMode );
        }

        auto *pContext = LoadDeviceContextFromRegistry( L );
        pContext->SetIndexBuffer( pIndexBuff, Offset, StateTransitionMode );

        // Return no values to Lua
        return 0;
    }
}
