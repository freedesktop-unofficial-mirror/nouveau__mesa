
/**
 * quad alpha test
 */

#include "sp_context.h"
#include "sp_headers.h"
#include "sp_quad.h"
#include "pipe/p_defines.h"
#include "pipe/p_util.h"


static void
alpha_test_quad(struct quad_stage *qs, struct quad_header *quad)
{
   struct softpipe_context *softpipe = qs->softpipe;
   const float ref = softpipe->depth_stencil->alpha.ref;
   unsigned passMask = 0x0, j;
   const float *aaaa = quad->outputs.color[3];

   switch (softpipe->depth_stencil->alpha.func) {
   case PIPE_FUNC_NEVER:
      quad->mask = 0x0;
      break;
   case PIPE_FUNC_LESS:
      /*
       * If mask were an array [4] we could do this SIMD-style:
       * passMask = (quad->outputs.color[3] <= vec4(ref));
       */
      for (j = 0; j < QUAD_SIZE; j++) {
         if (aaaa[j] < ref) {
            passMask |= (1 << j);
         }
      }
      break;
   case PIPE_FUNC_EQUAL:
      for (j = 0; j < QUAD_SIZE; j++) {
         if (aaaa[j] == ref) {
            passMask |= (1 << j);
         }
      }
      break;
   case PIPE_FUNC_LEQUAL:
      for (j = 0; j < QUAD_SIZE; j++) {
         if (aaaa[j] <= ref) {
            passMask |= (1 << j);
         }
      }
      break;
   case PIPE_FUNC_GREATER:
      for (j = 0; j < QUAD_SIZE; j++) {
         if (aaaa[j] > ref) {
            passMask |= (1 << j);
         }
      }
      break;
   case PIPE_FUNC_NOTEQUAL:
      for (j = 0; j < QUAD_SIZE; j++) {
         if (aaaa[j] != ref) {
            passMask |= (1 << j);
         }
      }
      break;
   case PIPE_FUNC_GEQUAL:
      for (j = 0; j < QUAD_SIZE; j++) {
         if (aaaa[j] >= ref) {
            passMask |= (1 << j);
         }
      }
      break;
   case PIPE_FUNC_ALWAYS:
      passMask = MASK_ALL;
      break;
   default:
      abort();
   }

   quad->mask &= passMask;

   if (quad->mask)
      qs->next->run(qs->next, quad);
}


static void alpha_test_begin(struct quad_stage *qs)
{
   qs->next->begin(qs->next);
}


static void alpha_test_destroy(struct quad_stage *qs)
{
   FREE( qs );
}


struct quad_stage *
sp_quad_alpha_test_stage( struct softpipe_context *softpipe )
{
   struct quad_stage *stage = CALLOC_STRUCT(quad_stage);

   stage->softpipe = softpipe;
   stage->begin = alpha_test_begin;
   stage->run = alpha_test_quad;
   stage->destroy = alpha_test_destroy;

   return stage;
}