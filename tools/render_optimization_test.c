/* Asset-free production GLES regression, including injected FBO failures. */
#include "render.h"
#include "rhi_gl.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int fail_fbo,fail_depth;
static void test_gen_depth(GLsizei n,GLuint *v) {
    if(fail_depth)memset(v,0,(size_t)n*sizeof *v);else glGenRenderbuffers(n,v);
}
static void test_gen_fbo(GLsizei n,GLuint *v) {
    if(fail_fbo)memset(v,0,(size_t)n*sizeof *v);else glGenFramebuffers(n,v);
}
#define glGenFramebuffers test_gen_fbo
#define glGenRenderbuffers test_gen_depth
#include "../src/rhi_gl.c"
#undef glGenFramebuffers
#undef glGenRenderbuffers

static unsigned char image[128*96*4],reference[sizeof image];
static const float black[]={0,0,0,1};
static const float projection[]={2,0,0,0,0,2,0,0,0,0,1,0,-1,-1,0,1};
static N2Batch rect(float x0,float y0,float x1,float y1,float z) {
    BatchedVertex v[4]={0};const float xy[][2]={{x0,y0},{x1,y0},{x1,y1},{x0,y1}};
    const uint16_t indices[]={0,1,2,0,2,3};N2Batch b={0};
    for(int i=0;i<4;i++){
        v[i].pos[0]=xy[i][0];v[i].pos[1]=xy[i][1];v[i].pos[2]=z;
        v[i].uv[0]=xy[i][0];v[i].uv[1]=xy[i][1];v[i].normal[2]=1;
        v[i].col[0]=96+i*8;v[i].col[1]=128;v[i].col[2]=144;v[i].col[3]=255;
    }
    b.vbo=rhi_buffer_create(&(RhiBufferDesc){RHI_BUFFER_VERTEX,RHI_UPDATE_STATIC,sizeof v,v});
    b.ibo=rhi_buffer_create(&(RhiBufferDesc){RHI_BUFFER_INDEX,RHI_UPDATE_STATIC,sizeof indices,indices});
    b.index_count=6;b.bbox_min[0]=x0;b.bbox_min[1]=y0;b.bbox_min[2]=z;
    b.bbox_max[0]=x1;b.bbox_max[1]=y1;b.bbox_max[2]=z;assert(b.vbo.id && b.ibo.id);return b;
}
static void release(N2Batch *b){rhi_buffer_destroy(b->vbo);rhi_buffer_destroy(b->ibo);}
static void flat(const RProg *r,const UG2WorldPipelines *p,const N2Batch *b,float red,float green,float blue) {
    r->p->uUnlit=1;r->p->uUseTex=0;r->p->uFogDensity=0;
    ug2_copy(r->p->uMVP,projection,16);ug2_vec3(r->p->uColor,red,green,blue);
    rhi_cmd_bind_pipeline(p->opaque);render_draw_batch(b);
}
static void read_image(void){assert(rhi_read_pixels(0,0,128,96,RHI_READ_RGBA8,image,0)==RHI_OK);}
static void scene_tests(const RProg *r,const UG2WorldPipelines *p,SDL_Window *window) {
    SceneTarget s={0};assert(rhi_caps()->depth24_target);
    assert(render_scene_begin(&s,64,64,black));
    const RhiGlTexture *depth=texture_find(s.depth);assert(depth && depth->format==RHI_FORMAT_D24);
    glBindRenderbuffer(GL_RENDERBUFFER,depth->name);GLint bits=0;
    glGetRenderbufferParameteriv(GL_RENDERBUFFER,GL_RENDERBUFFER_DEPTH_SIZE,&bits);assert(bits==24);
    N2Batch bottom=rect(0,0,1,.5f,0),top=rect(0,.5f,1,1,0);
    flat(r,p,&bottom,0,0,1);flat(r,p,&top,1,0,0);
    render_scene_composite(&s,black);read_image();
    assert(image[(10*128+64)*4]==255 && image[(85*128+64)*4+2]==255); /* top-down readback */
    assert(image[(47*128+64)*4]>0 && image[(47*128+64)*4]<255); /* bilinear edge */
    /* The native HUD can occupy a single physical pixel after the upscale. */
    N2Batch hud=rect(32.f/128,0,33.f/128,1,-.5f);flat(r,p,&hud,0,1,0);read_image();
    for(int y=0;y<96;y++){
        assert(image[(y*128+32)*4+1]==255);
        assert(image[(y*128+31)*4+1]==0 && image[(y*128+33)*4+1]==0);
    }
    RhiTexture color=s.color,d=s.depth;RhiBuffer vertices=s.vertices;
    assert(render_scene_begin(&s,64,64,black));assert(s.color.id==color.id && s.depth.id==d.id && s.vertices.id==vertices.id);
    /* Two overlapping surfaces that collapse to one D16 depth value. */
    N2Batch near=rect(0,0,1,1,-.000004f),far=rect(0,0,1,1,0);
    flat(r,p,&near,0,1,0);flat(r,p,&far,1,0,0);
    render_scene_composite(&s,black);read_image();assert(image[(48*128+64)*4+1]==255);
    assert(render_scene_begin(&s,96,72,black));assert(s.color.id!=color.id && s.depth.id!=d.id);
    assert(!texture_find(color) && !texture_find(d));render_scene_composite(&s,black);
    rhi_frame_end();SDL_SetWindowSize(window,160,120);SDL_PumpEvents();RhiFrameInfo frame;
    assert(rhi_frame_begin(&frame)==RHI_OK && frame.width==160 && frame.height==120);
    assert(render_scene_begin(&s,96,72,black));render_scene_composite(&s,black);
    GLint viewport[4];glGetIntegerv(GL_VIEWPORT,viewport);assert(viewport[2]==160 && viewport[3]==120);
    color=s.color;d=s.depth;render_scene_free(&s);assert(!texture_find(color) && !texture_find(d));
    assert(!s.color.id && !s.depth.id && !s.vertices.id && !s.shader.id);
    /* FBO failure after colour/depth/shader allocation frees partial resources. */
    fail_fbo=1;assert(!render_scene_begin(&s,64,64,black));fail_fbo=0;
    assert(s.failed && !s.color.id && !s.depth.id && !s.vertices.id && !s.shader.id);
    assert(rhi_cmd_clear_main(black,1)==RHI_OK);assert(!render_scene_begin(&s,64,64,black));render_scene_free(&s);
    fail_depth=1;assert(!render_scene_begin(&s,64,64,black));fail_depth=0;
    assert(s.failed && !s.color.id && !s.depth.id);render_scene_free(&s);
    g_rhi.caps.depth24_target=0;assert(!render_scene_begin(&s,64,64,black));assert(!s.depth.id);
    g_rhi.caps.depth24_target=1;render_scene_free(&s);
    release(&bottom);release(&top);release(&near);release(&far);release(&hud);
    rhi_frame_end();SDL_SetWindowSize(window,128,96);SDL_PumpEvents();assert(rhi_frame_begin(NULL)==RHI_OK);
    puts("PASS: bilinear composition, native pixel HUD, orientation, D24 overlapping surfaces, reuse/resize, partial failure and cleanup");
}
static void parity_tests(void) {
    N2Batch b=rect(0,0,1,1,0);
    unsigned char rgb[]={70,80,90, 160,170,180, 90,120,80, 120,100,130},alpha[]={255,0,255,255};
    N2Tex tex={.w=2,.h=2,.rgb=rgb,.alpha=alpha};b.tex=upload_tex(&tex);assert(b.tex.id);
    for(int profile=0;profile<4;profile++){
        fprintf(stderr,"parity profile %d\n",profile);
        RProg legacy=render_program_variant(profile,0),world=render_program_variant(profile,1);
        assert(legacy.shader.id && world.shader.id && legacy.p!=world.p);
        UG2WorldPipelines lp,wp;render_world_pipelines(&legacy,&lp);render_world_pipelines(&world,&wp);
        rhi_cmd_bind_pipeline(lp.opaque);
        legacy.p->uAmbient=.22f;legacy.p->uDiffuse=.44f;legacy.p->uVColor=1;
        legacy.p->uFogDensity=.02f;ug2_vec3(legacy.p->uCamPos,0,0,4);
        ug2_copy(legacy.p->uMVP,projection,16);
        const float model[]={1,0,0,0,0,1,0,0,0,0,1,0,-3,.5f,.7f,1};
        const float anchors[4][4]={{0,.2f,0,1},{0,-.2f,0,1},{0},{0}};
        HeadlightShadows shadows={.size=profile==0?512:256};
        for(int high=0;high<2;high++)for(int cutout=0;cutout<2;cutout++){
            b.drawmode=cutout?N2_DRAW_CUTOUT:N2_DRAW_OPAQUE;
            render_headlights(&legacy,model,anchors,high,high?1:4,high?85:35,1);
            if(profile!=3)assert(render_headlight_shadows(&legacy,&shadows,NULL,0)==0);
            else assert(!shadows.map[0].id && !shadows.depth.id);
            for(int specialized=0;specialized<2;specialized++){
                assert(rhi_cmd_clear_main(black,1)==RHI_OK);
                assert(!render_world_batch(&legacy,&world,&lp,&wp,&b,0,specialized));
                GLenum error=glGetError();if(error)fprintf(stderr,"draw error profile=%d high=%d cutout=%d specialized=%d gl=%x\n",profile,high,cutout,specialized,error);assert(!error);
                read_image();
                if(!specialized){int lit=0;for(size_t i=0;i<sizeof image;i+=4)lit+=image[i]>0;assert(lit>100);memcpy(reference,image,sizeof image);}
                else for(size_t i=0;i<sizeof image;i++)assert(abs((int)image[i]-reference[i])<=1);
            }
        }
        assert(glGetError()==GL_NO_ERROR);
        b.wettable=1;assert(render_world_batch(&legacy,&world,&lp,&wp,&b,1,1)==1);b.wettable=0;
        RProg missing={0};assert(render_world_batch(&legacy,&missing,&lp,&wp,&b,0,1)==1);
        assert(glGetError()==GL_NO_ERROR);
        free_headlight_shadows(&shadows);rhi_shader_destroy(world.shader);rhi_shader_destroy(legacy.shader);
    }
    rhi_texture_destroy(b.tex);release(&b);
    puts("PASS: general/specialized <=1 channel level, opaque/cutout/prelight/fog/headlights, all profiles and fallback");
}
static void queue_tests(void) {
    WorldDrawQueue q={0};assert(render_world_queue_reserve(&q,8));WorldDrawEntry *storage=q.entries;
    N2Batch b={0};float cam[]={0,0,0};b.bbox_max[0]=b.bbox_max[1]=b.bbox_max[2]=1;
    b.tex.id=4;render_world_queue_push(&q,&b,3,cam);render_world_queue_push(&q,&b,1,cam);
    b.tex.id=2;render_world_queue_push(&q,&b,2,cam);
    b.bbox_min[0]=b.bbox_max[0]=64;render_world_queue_push(&q,&b,0,cam);
    render_world_queue_sort(&q);
    assert(q.count==4 && q.entries[0].index==2 && q.entries[1].index==1 && q.entries[2].index==3 && q.entries[3].index==0);
    unsigned mask=0;for(int i=0;i<q.count;i++)mask|=1u<<q.entries[i].index;assert(mask==15);
    q.count=0;b.bbox_min[0]=b.bbox_max[0]=16;render_world_queue_push(&q,&b,0,cam);assert(q.entries[0].bucket==0);
    assert(render_world_queue_reserve(&q,8) && q.entries==storage);free(q.entries);
    puts("PASS: identical queued visibility, 32m buckets, state grouping, coplanar stable ties and moved bounds, reused storage");
}
int main(void) {
    assert(SDL_Init(SDL_INIT_VIDEO)==0);assert(rhi_prepare_window(RHI_BACKEND_DEFAULT)==RHI_OK);
    SDL_Window *window=SDL_CreateWindow("render optimization",0,0,128,96,rhi_window_flags(RHI_BACKEND_DEFAULT)|SDL_WINDOW_HIDDEN|SDL_WINDOW_RESIZABLE);assert(window);
    assert(rhi_device_create(&(RhiDeviceDesc){RHI_BACKEND_DEFAULT,0,window})==RHI_OK);
    assert(rhi_frame_begin(NULL)==RHI_OK);RProg r=render_program();assert(r.shader.id);
    UG2WorldPipelines p;render_world_pipelines(&r,&p);scene_tests(&r,&p,window);
    rhi_shader_destroy(r.shader);parity_tests();queue_tests();
    assert(glGetError()==GL_NO_ERROR);rhi_frame_end();rhi_device_destroy();SDL_DestroyWindow(window);SDL_Quit();return 0;
}
