#ifndef _WIN32
#include <unistd.h>
#endif
#include "rhi_gl.h"
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
/* Raises GL_INVALID_ENUM through a call the state tracker does not model, as
 * an earlier unrelated call would. */
static void pending_error(void) { glHint(0x1234, GL_NICEST); }
int main(void) {
    const float green[4]={0,1,0,1};
    assert(SDL_Init(SDL_INIT_VIDEO)==0);
    assert(rhi_prepare_window(RHI_BACKEND_DEFAULT)==RHI_OK);
    SDL_Window *window=SDL_CreateWindow("frame review",0,0,64,64,
        rhi_window_flags(RHI_BACKEND_DEFAULT)|SDL_WINDOW_HIDDEN); assert(window);
    RhiDeviceDesc device={RHI_BACKEND_DEFAULT,0,window};
    assert(rhi_device_create(&device)==RHI_OK);
    const RhiCaps *caps=rhi_caps();
    const int gpu_timer=caps->gpu_timer;
    assert(caps->max_texture_size>=64);

    /* Main-target clear: whole frame, regardless of masks, scissor and viewport. */
    assert(rhi_cmd_clear_main(green,1.0f)==RHI_ERR_INVALID);   /* outside a frame */
    assert(rhi_frame_begin(NULL)==RHI_OK);
    assert(rhi_cmd_clear_main(NULL,1.0f)==RHI_ERR_INVALID);
    glClearColor(0,0,1,1);glClear(GL_COLOR_BUFFER_BIT);
    glColorMask(GL_FALSE,GL_TRUE,GL_FALSE,GL_TRUE);glDepthMask(GL_FALSE);
    glEnable(GL_SCISSOR_TEST);glScissor(0,0,8,8);glViewport(0,0,16,16);
    assert(rhi_cmd_clear_main(green,1.0f)==RHI_OK);
    unsigned char px[64*64*3];
    assert(rhi_read_pixels(0,0,64,64,RHI_READ_RGB8,px,0)==RHI_OK);
    for(int i=0;i<64*64;i++)assert(px[i*3]==0 && px[i*3+1]==255 && px[i*3+2]==0);
    GLboolean cm[4],dm;GLint vp[4];
    gls_get_booleanv(GL_COLOR_WRITEMASK,cm);gls_get_booleanv(GL_DEPTH_WRITEMASK,&dm);
    assert(!cm[0] && cm[1] && !cm[2] && cm[3] && !dm);   /* masks come back */
    gls_get_integerv(GL_VIEWPORT,vp);
    assert(vp[0]==0 && vp[1]==0 && vp[2]==64 && vp[3]==64 && !gls_is_enabled(GL_SCISSOR_TEST));
    glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glDepthMask(GL_TRUE);
    /* Not inside a pass. */
    RhiTexture target=rhi_texture_create(&(RhiTextureDesc){RHI_TEXTURE_2D,RHI_FORMAT_RGBA8,4,4,
        RHI_TEXTURE_SAMPLED|RHI_TEXTURE_COLOR_TARGET});
    assert(target.id && rhi_texture_upload(target,0,0,NULL,0)==RHI_OK);
    assert(rhi_cmd_begin_pass(&(RhiPassDesc){{target,0},{{0},0},RHI_LOAD_CLEAR,RHI_LOAD_KEEP,{0,0,0,1},1})==RHI_OK);
    assert(rhi_cmd_clear_main(green,1.0f)==RHI_ERR_INVALID);
    GLint offscreen_fbo;glGetIntegerv(GL_FRAMEBUFFER_BINDING,&offscreen_fbo);assert(offscreen_fbo);
    rhi_cmd_end_pass();
    /* Ending a pass leaves its target bound; clear_main must select the main target. */
    assert(rhi_cmd_clear_main(green,1.0f)==RHI_OK);
    GLint main_fbo;glGetIntegerv(GL_FRAMEBUFFER_BINDING,&main_fbo);assert(main_fbo==0);
    assert(rhi_read_pixels(0,0,64,64,RHI_READ_RGB8,px,0)==RHI_OK);
    for(int i=0;i<64*64;i++)assert(px[i*3]==0 && px[i*3+1]==255 && px[i*3+2]==0);
    glBindFramebuffer(GL_FRAMEBUFFER,(GLuint)offscreen_fbo);
    unsigned char offscreen[4];glReadPixels(0,0,1,1,GL_RGBA,GL_UNSIGNED_BYTE,offscreen);
    assert(!offscreen[0] && !offscreen[1] && !offscreen[2]);
    glBindFramebuffer(GL_FRAMEBUFFER,0);
    /* Clear ignores but preserves both stencil write masks. */
    glStencilMaskSeparate(GL_FRONT,0x12);glStencilMaskSeparate(GL_BACK,0x34);
    assert(rhi_cmd_clear_main(green,1.0f)==RHI_OK);
    GLint front_mask,back_mask;
    glGetIntegerv(GL_STENCIL_WRITEMASK,&front_mask);glGetIntegerv(GL_STENCIL_BACK_WRITEMASK,&back_mask);
    assert(front_mask==0x12 && back_mask==0x34);
    glStencilMask(0xFF);
    assert(glGetError()==GL_NO_ERROR);

    /* Upload failures come back as results. */
#ifndef _WIN32
    /* The failure line names the GL error and says it was already pending. */
    pending_error();
    {   char log[512]={0};FILE*cap=tmpfile();assert(cap);fflush(stderr);
        int saved=dup(2);assert(saved>=0&&dup2(fileno(cap),2)>=0);
        RhiBuffer stale=rhi_buffer_create(&(RhiBufferDesc){RHI_BUFFER_VERTEX,RHI_UPDATE_STATIC,16,NULL});
        fflush(stderr);dup2(saved,2);close(saved);
        rewind(cap);size_t got=fread(log,1,sizeof log-1,cap);log[got]=0;fclose(cap);
        assert(!stale.id && glGetError()==GL_NO_ERROR);
        fprintf(stderr,"%s",log);
        assert(strstr(log,"already pending") && strstr(log,"GL_INVALID_"));
    }
#endif
    pending_error();
    RhiBuffer failed=rhi_buffer_create(&(RhiBufferDesc){RHI_BUFFER_VERTEX,RHI_UPDATE_STATIC,16,NULL});
    assert(!failed.id && glGetError()==GL_NO_ERROR);
    RhiBuffer stat=rhi_buffer_create(&(RhiBufferDesc){RHI_BUFFER_VERTEX,RHI_UPDATE_STATIC,16,NULL});
    assert(stat.id);
    pending_error();assert(rhi_buffer_update(stat,0,4,px)==RHI_ERR_UNSUPPORTED);
    pending_error();assert(rhi_buffer_replace(stat,32,NULL)==RHI_ERR_UNSUPPORTED);
    assert(rhi_buffer_update(stat,0,32,px)==RHI_OK);
    /* STREAM buffers do not check: the error stays for the caller. */
    RhiBuffer stream=rhi_buffer_create(&(RhiBufferDesc){RHI_BUFFER_VERTEX,RHI_UPDATE_STREAM,0,NULL});
    assert(stream.id);
    pending_error();assert(rhi_buffer_replace(stream,16,px)==RHI_OK);
    assert(glGetError()==GL_INVALID_ENUM && glGetError()==GL_NO_ERROR);
    rhi_buffer_destroy(stat);rhi_buffer_destroy(stream);
    /* A level larger than the device allows is rejected by GL. */
    RhiTexture big=rhi_texture_create(&(RhiTextureDesc){RHI_TEXTURE_2D,RHI_FORMAT_RGBA8,
        caps->max_texture_size+1,1,RHI_TEXTURE_SAMPLED});
    assert(big.id && rhi_texture_upload(big,0,0,NULL,0)==RHI_ERR_UNSUPPORTED);
    rhi_texture_destroy(big);
    RhiTexture tex=rhi_texture_create(&(RhiTextureDesc){RHI_TEXTURE_2D,RHI_FORMAT_RGB8,4,4,RHI_TEXTURE_SAMPLED});
    pending_error();assert(rhi_texture_upload(tex,0,0,px,0)==RHI_ERR_UNSUPPORTED);
    assert(rhi_texture_upload(tex,0,0,px,0)==RHI_OK);
    pending_error();assert(rhi_texture_generate_mips(tex)==RHI_ERR_UNSUPPORTED);
    assert(rhi_texture_generate_mips(tex)==RHI_OK);
    rhi_texture_destroy(tex);rhi_texture_destroy(target);
    assert(glGetError()==GL_NO_ERROR);

    /* State snapshots: everything comes back, outputs set on the snapshot win. */
    {
        RhiTexture t0=rhi_texture_create(&(RhiTextureDesc){RHI_TEXTURE_2D,RHI_FORMAT_RGB8,1,1,RHI_TEXTURE_SAMPLED});
        RhiTexture t1=rhi_texture_create(&(RhiTextureDesc){RHI_TEXTURE_2D,RHI_FORMAT_RGB8,1,1,RHI_TEXTURE_SAMPLED});
        assert(t0.id && t1.id);
        rhi_cmd_bind_texture(0,t0);rhi_cmd_bind_texture(3,t1);glActiveTexture(GL_TEXTURE2);
        glEnable(GL_CULL_FACE);glEnable(GL_STENCIL_TEST);glDisable(GL_DEPTH_TEST);glEnable(GL_BLEND);
        glBlendFuncSeparate(GL_ONE,GL_ONE,GL_ZERO,GL_ONE);glDepthFunc(GL_GREATER);glDepthMask(GL_FALSE);
        glColorMask(GL_TRUE,GL_FALSE,GL_TRUE,GL_FALSE);glViewport(1,2,30,40);glClearColor(.25f,.5f,.75f,1);
        RhiStateSnapshot s0;rhi_cmd_save_state(&s0);
        glDisable(GL_CULL_FACE);glDisable(GL_STENCIL_TEST);glEnable(GL_DEPTH_TEST);glDisable(GL_BLEND);
        glBlendFunc(GL_ONE,GL_ZERO);glDepthFunc(GL_LESS);glDepthMask(GL_TRUE);glColorMask(1,1,1,1);
        glViewport(0,0,64,64);glClearColor(0,0,0,0);
        rhi_cmd_bind_texture(0,t1);rhi_cmd_bind_texture(3,t0);rhi_cmd_bind_texture(1,t0);
        rhi_cmd_restore_state(&s0);
        GLint v[4];GLboolean b[4];GLfloat f[4];
        assert(gls_is_enabled(GL_CULL_FACE) && gls_is_enabled(GL_STENCIL_TEST) &&
               !gls_is_enabled(GL_DEPTH_TEST) && gls_is_enabled(GL_BLEND));
        gls_get_integerv(GL_BLEND_SRC_RGB,v);assert(v[0]==GL_ONE);
        gls_get_integerv(GL_BLEND_SRC_ALPHA,v);assert(v[0]==GL_ZERO);
        gls_get_integerv(GL_DEPTH_FUNC,v);assert(v[0]==GL_GREATER);
        gls_get_booleanv(GL_DEPTH_WRITEMASK,b);assert(!b[0]);
        gls_get_booleanv(GL_COLOR_WRITEMASK,b);assert(b[0] && !b[1] && b[2] && !b[3]);
        gls_get_integerv(GL_VIEWPORT,v);assert(v[0]==1 && v[1]==2 && v[2]==30 && v[3]==40);
        gls_get_floatv(GL_COLOR_CLEAR_VALUE,f);assert(f[0]==.25f && f[2]==.75f);
        gls_get_integerv(GL_ACTIVE_TEXTURE,v);assert(v[0]==GL_TEXTURE2);
        assert(gls_texture_2d(0)==(GLint)rhi_gl_texture_name(t0) && gls_texture_2d(1)==0 &&
               gls_texture_2d(3)==(GLint)rhi_gl_texture_name(t1));
        /* The real GL state agrees with the tracker. */
        glGetIntegerv(GL_ACTIVE_TEXTURE,v);assert(v[0]==GL_TEXTURE2);
        glActiveTexture(GL_TEXTURE3);glGetIntegerv(GL_TEXTURE_BINDING_2D,v);
        assert((GLuint)v[0]==rhi_gl_texture_name(t1));
        /* An output texture replaces what the snapshot holds on its slot. */
        rhi_state_set_texture(&s0,1,t1);rhi_cmd_restore_state(&s0);
        assert(gls_texture_2d(1)==(GLint)rhi_gl_texture_name(t1));
        gls_get_integerv(GL_ACTIVE_TEXTURE,v);assert(v[0]==GL_TEXTURE2);
        rhi_state_set_texture(&s0,99,t1);   /* out of range: ignored */
        /* A viewport set through rhi comes back too; a negative size is rejected. */
        assert(rhi_cmd_set_viewport(5,6,7,8)==RHI_OK);
        gls_get_integerv(GL_VIEWPORT,v);assert(v[0]==5 && v[1]==6 && v[2]==7 && v[3]==8);
        glGetIntegerv(GL_VIEWPORT,v);assert(v[0]==5 && v[1]==6 && v[2]==7 && v[3]==8);
        assert(rhi_cmd_set_viewport(0,0,-1,8)==RHI_ERR_INVALID);
        gls_get_integerv(GL_VIEWPORT,v);assert(v[2]==7);
        rhi_cmd_restore_state(&s0);
        gls_get_integerv(GL_VIEWPORT,v);assert(v[0]==1 && v[1]==2 && v[2]==30 && v[3]==40);
        rhi_texture_destroy(t0);rhi_texture_destroy(t1);
        glDisable(GL_CULL_FACE);glDisable(GL_STENCIL_TEST);glDisable(GL_BLEND);glDepthMask(GL_TRUE);
        glColorMask(1,1,1,1);glActiveTexture(GL_TEXTURE0);
        assert(glGetError()==GL_NO_ERROR);
    }

    /* Vertex-array state: attribute descriptions and the element-buffer binding come back too (hud_draw's contract). */
    {
        GLuint ab[2],eb[2];glGenBuffers(2,ab);glGenBuffers(2,eb);
        glBindBuffer(GL_ARRAY_BUFFER,ab[0]);glBufferData(GL_ARRAY_BUFFER,256,NULL,GL_STATIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER,ab[1]);glBufferData(GL_ARRAY_BUFFER,256,NULL,GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,eb[0]);glBufferData(GL_ELEMENT_ARRAY_BUFFER,64,NULL,GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,eb[1]);glBufferData(GL_ELEMENT_ARRAY_BUFFER,64,NULL,GL_STATIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER,ab[0]);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,eb[0]);
        glEnableVertexAttribArray(0);glEnableVertexAttribArray(3);glDisableVertexAttribArray(5);
        glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,12,(const void*)12);
        glBindBuffer(GL_ARRAY_BUFFER,ab[1]);
        glVertexAttribPointer(3,4,GL_UNSIGNED_BYTE,GL_TRUE,16,(const void*)4);
        glBindBuffer(GL_ARRAY_BUFFER,ab[0]);
        RhiStateSnapshot sv;rhi_cmd_save_state(&sv);
        /* disturb: other buffers, other descriptions, other enable bits, other element buffer */
        glBindBuffer(GL_ARRAY_BUFFER,ab[1]);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,eb[1]);
        glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,24,(const void*)0);
        glVertexAttribPointer(3,2,GL_FLOAT,GL_FALSE,8,(const void*)8);
        glDisableVertexAttribArray(3);glEnableVertexAttribArray(5);
        rhi_cmd_restore_state(&sv);
        GLint iv;void *pv;
        /* tracker */
        gls_get_integerv(GL_ARRAY_BUFFER_BINDING,&iv);assert(iv==(GLint)ab[0]);
        gls_get_integerv(GL_ELEMENT_ARRAY_BUFFER_BINDING,&iv);assert(iv==(GLint)eb[0]);
        assert(gls_attrib_enabled(0) && gls_attrib_enabled(3) && !gls_attrib_enabled(5));
        GlsAttribPointer ap;gls_attrib_pointer(0,&ap);
        assert(ap.size==3 && ap.type==(GLint)GL_FLOAT && !ap.normalized && ap.stride==12 && ap.buffer==(GLint)ab[0] && (uintptr_t)ap.pointer==12);
        gls_attrib_pointer(3,&ap);
        assert(ap.size==4 && ap.type==(GLint)GL_UNSIGNED_BYTE && ap.normalized && ap.stride==16 && ap.buffer==(GLint)ab[1] && (uintptr_t)ap.pointer==4);
        /* real GL agrees */
        glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&iv);assert(iv==(GLint)ab[0]);
        glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING,&iv);assert(iv==(GLint)eb[0]);
        glGetVertexAttribiv(0,GL_VERTEX_ATTRIB_ARRAY_SIZE,&iv);assert(iv==3);
        glGetVertexAttribiv(0,GL_VERTEX_ATTRIB_ARRAY_STRIDE,&iv);assert(iv==12);
        glGetVertexAttribiv(0,GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING,&iv);assert(iv==(GLint)ab[0]);
        glGetVertexAttribPointerv(0,GL_VERTEX_ATTRIB_ARRAY_POINTER,&pv);assert((uintptr_t)pv==12);
        glGetVertexAttribiv(3,GL_VERTEX_ATTRIB_ARRAY_TYPE,&iv);assert(iv==(GLint)GL_UNSIGNED_BYTE);
        glGetVertexAttribiv(3,GL_VERTEX_ATTRIB_ARRAY_NORMALIZED,&iv);assert(iv);
        glGetVertexAttribiv(3,GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING,&iv);assert(iv==(GLint)ab[1]);
        glGetVertexAttribiv(3,GL_VERTEX_ATTRIB_ARRAY_ENABLED,&iv);assert(iv);
        glGetVertexAttribiv(5,GL_VERTEX_ATTRIB_ARRAY_ENABLED,&iv);assert(!iv);
        glDisableVertexAttribArray(0);glDisableVertexAttribArray(3);glBindBuffer(GL_ARRAY_BUFFER,0);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);
        /* Tightly packed strides: 0 and size*typesize are the same fetch, and a driver may report either (macOS reports 16 for a default
           FLOAT4 attribute). The comparison treats them as equal, and restoring a default attribute twice stays consistent. */
        assert(gls_attrib_effective_stride(4,GL_FLOAT,0)==16 && gls_attrib_effective_stride(4,GL_FLOAT,16)==16);
        assert(gls_attrib_effective_stride(3,GL_UNSIGNED_BYTE,0)==3 && gls_attrib_effective_stride(2,GL_SHORT,0)==4);
        assert(gls_attrib_effective_stride(4,GL_FLOAT,24)==24 && gls_attrib_effective_stride(4,GL_FLOAT,0)!=gls_attrib_effective_stride(4,GL_FLOAT,24));
        for(int pass=0;pass<2;pass++){
            GlsAttribPointer d;gls_attrib_pointer(6,&d);   /* never configured: GL defaults */
            assert(d.size==4 && d.type==(GLint)GL_FLOAT && d.stride==0 && d.buffer==0 && !d.pointer);
            RhiStateSnapshot dv;rhi_cmd_save_state(&dv);
            glVertexAttribPointer(6,2,GL_FLOAT,GL_FALSE,8,(const void*)0);
            rhi_cmd_restore_state(&dv);
            gls_attrib_pointer(6,&d);
            assert(d.size==4 && d.type==(GLint)GL_FLOAT && d.stride==0 && !d.pointer);
        }
        glDeleteBuffers(2,ab);glDeleteBuffers(2,eb);
        assert(glGetError()==GL_NO_ERROR);
    }

    /* GPU timers. */
    uint64_t ns=0;
    if(caps->gpu_timer) {
        RhiTimer t=rhi_timer_create(),u=rhi_timer_create();assert(t.id && u.id && t.id!=u.id);
        assert(rhi_cmd_timer_end()==RHI_ERR_INVALID);
        assert(rhi_cmd_timer_begin(t)==RHI_OK);
        assert(rhi_cmd_timer_begin(u)==RHI_ERR_INVALID);   /* one at a time */
        assert(!rhi_timer_read(t,&ns));                     /* still running */
        assert(rhi_cmd_clear_main(green,1.0f)==RHI_OK);
        assert(rhi_cmd_timer_end()==RHI_OK);
        glFinish();
        int ready=0;
        for(int i=0;i<1000 && !ready;i++)ready=rhi_timer_read(t,&ns);
        assert(ready);
        assert(!rhi_timer_read(u,&ns));   /* never measured */
        rhi_timer_destroy(t);assert(!rhi_timer_read(t,&ns));
        assert(rhi_cmd_timer_begin(t)==RHI_ERR_INVALID);
        RhiTimer again=rhi_timer_create();assert(again.id && again.id!=t.id);
        rhi_timer_destroy(again);rhi_timer_destroy(u);
    } else {
        assert(!rhi_timer_create().id);
    }
    assert(glGetError()==GL_NO_ERROR);
    /* A reduced profiling frame must read the same lower-left region it
     * renders, not the upper-left rectangle of the physical drawable. */
    assert(rhi_profile_main_size(0,32)==RHI_ERR_INVALID);
    assert(rhi_profile_main_size(32,32)==RHI_OK);
    assert(rhi_cmd_clear_main(green,1)==RHI_OK);
    gls_get_integerv(GL_VIEWPORT,vp);
    assert(vp[2]==32 && vp[3]==32);
    glEnable(GL_SCISSOR_TEST);glScissor(0,0,32,32);
    glClearColor(0,0,1,1);glClear(GL_COLOR_BUFFER_BIT);glDisable(GL_SCISSOR_TEST);
    assert(rhi_read_pixels(0,0,32,32,RHI_READ_RGB8,px,0)==RHI_OK);
    for(int i=0;i<32*32;i++)assert(px[i*3]==0 && px[i*3+1]==0 && px[i*3+2]==255);
    assert(rhi_profile_finish()==RHI_OK);
    assert(rhi_frame_end()==RHI_OK);
    assert(rhi_profile_main_size(32,32)==RHI_ERR_INVALID);
    RhiFrameInfo next_frame;
    assert(rhi_frame_begin(&next_frame)==RHI_OK);
    assert(next_frame.width==64 && next_frame.height==64);
    assert(rhi_frame_end()==RHI_OK);
    rhi_device_destroy();SDL_DestroyWindow(window);SDL_Quit();
    printf("rhi_frame_test: PASS (main clear, upload results, texture limit, state snapshots, gpu timer %s)\n",
           gpu_timer?"on":"unavailable");
}
