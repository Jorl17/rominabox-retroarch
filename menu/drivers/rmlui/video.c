/* ROM-in-a-Box: the brightness and contrast that the player sets on VIDEO.
 *
 * We apply both in a pass that we add after the shader the player chose. In
 * the exporter we write the pass in the game's shader language, with the
 * shader parameters that we name in settings.inc (RIB_SETTING_PARAMETER), and
 * we name its preset in shaders.cfg. While every one of those settings is at
 * 1.0, at which the picture stays as it is, we leave the pass out. Otherwise
 * we write the chosen preset with our pass after it into the game's data,
 * with the values of the settings, and apply that preset. When we start the
 * video driver again, as for fullscreen on Windows, we load that file, so we
 * write it again on every change and also set the values on the running
 * shader. */
#include "video.h"
#include "host.h"
#include "../../../configuration.h"
#include "../../../gfx/video_driver.h"
#include "../../../gfx/video_shader_parse.h"
#include "../../../retroarch.h"
#include "../../../verbosity.h"
#include <string/stdstring.h>
#include <stdlib.h>
#include <string.h>

static char pass_preset[PATH_MAX_LENGTH];
static char written_preset[PATH_MAX_LENGTH];
static char chosen_preset[PATH_MAX_LENGTH];
static bool chosen_known;
static bool running_with_pass;

void rib_host_video_pass(const char *pass, const char *written)
{
   strlcpy(pass_preset, pass ? pass : "", sizeof(pass_preset));
   strlcpy(written_preset, written ? written : "", sizeof(written_preset));
}

/* Whether a setting is away from 1.0, so that we change the picture with the pass. */
static bool rib_video_pass_needed(void)
{
   float value;
   if (!*pass_preset || !*written_preset)
      return false;
#define RIB_SETTING_PARAMETER(name, parameter) \
   if (rib_host_setting(RIB_SETTING_##name, &value) && value != 1.0f) \
      return true;
#include "settings.inc"
   return false;
}

/* Give the parameters of our pass in `shader` the values of their settings. */
static void rib_video_set_values(struct video_shader *shader)
{
   unsigned index;
   float value;
   for (index = 0; index < shader->num_parameters; index++)
   {
#define RIB_SETTING_PARAMETER(name, parameter) \
      if (string_is_equal(shader->parameters[index].id, parameter) \
            && rib_host_setting(RIB_SETTING_##name, &value)) \
         shader->parameters[index].current = value;
#include "settings.inc"
   }
}

/* Write the chosen preset, or none, with our pass after it to written_preset.
 *
 * In a preset without a scale for a pass, we draw the last pass at the size
 * of the window with RetroArch, and any other at the size of the game. The
 * last pass of the chosen preset is no longer last, so we give it the size of
 * the window, at which we drew it before. When we find the parameters of
 * every pass again, we set them to their defaults, so we keep the values of
 * its parameters from the chosen preset. */
static bool rib_video_write(void)
{
   struct video_shader *shader = (struct video_shader*)calloc(1, sizeof(*shader));
   struct video_shader *pass   = (struct video_shader*)calloc(1, sizeof(*pass));
   struct video_shader_parameter *kept = NULL;
   unsigned kept_count = 0, index, other;
   bool written = false;

   if (!shader || !pass)
      goto done;
   if (*chosen_preset && !video_shader_load_preset_into_shader(chosen_preset, shader))
      goto done;
   if (!video_shader_load_preset_into_shader(pass_preset, pass) || pass->passes != 1
         || shader->passes >= GFX_MAX_SHADERS)
      goto done;
   if (shader->passes > 0)
   {
      struct gfx_fbo_scale *scale = &shader->pass[shader->passes - 1].fbo;
      if (!(scale->flags & FBO_SCALE_FLAG_VALID))
      {
         scale->type_x  = RARCH_SCALE_VIEWPORT;
         scale->type_y  = RARCH_SCALE_VIEWPORT;
         scale->scale_x = 1.0f;
         scale->scale_y = 1.0f;
         scale->flags  |= FBO_SCALE_FLAG_VALID;
      }
   }
   if (shader->num_parameters > 0)
   {
      kept = (struct video_shader_parameter*)malloc(
            shader->num_parameters * sizeof(*kept));
      if (!kept)
         goto done;
      kept_count = shader->num_parameters;
      memcpy(kept, shader->parameters, kept_count * sizeof(*kept));
   }
   shader->pass[shader->passes++] = pass->pass[0];
   video_shader_resolve_parameters(shader);
   for (index = 0; index < shader->num_parameters; index++)
      for (other = 0; other < kept_count; other++)
         if (string_is_equal(shader->parameters[index].id, kept[other].id))
            shader->parameters[index].current = kept[other].current;
   rib_video_set_values(shader);
   written = video_shader_write_preset(written_preset, shader, false);

done:
   if (!written)
      RARCH_ERR("[RIB] could not write %s with the brightness and contrast pass "
            "after '%s'.\n", written_preset, chosen_preset);
   free(kept);
   free(pass);
   free(shader);
   return written;
}

/* Apply the chosen preset, with our pass after it when a setting is away from 1.0. */
static void rib_video_apply(void)
{
   settings_t *settings = config_get_ptr();
   const bool with_pass = rib_video_pass_needed() && rib_video_write();
   const char *preset   = with_pass ? written_preset : chosen_preset;
   bool applied;
   if (!settings)
      return;
   configuration_set_bool(settings, settings->bools.video_shader_enable, *preset != '\0');
   applied = video_shader_apply_shader(settings,
         *preset ? video_shader_parse_type(preset) : RARCH_SHADER_NONE,
         *preset ? preset : NULL, false);
   running_with_pass = applied && with_pass;
   RARCH_LOG("[RIB] shader %s: %s%s\n", applied ? "applied" : "not applied",
         *chosen_preset ? chosen_preset : "unfiltered",
         running_with_pass ? ", with the brightness and contrast pass" : "");
}

void rib_video_start(void)
{
   const char *running = video_shader_get_current_shader_preset();
   strlcpy(chosen_preset, running ? running : "", sizeof(chosen_preset));
   chosen_known = true;
   if (rib_video_pass_needed())
      rib_video_apply();
}

void rib_video_choose(const char *preset)
{
   strlcpy(chosen_preset, preset ? preset : "", sizeof(chosen_preset));
   chosen_known = true;
   rib_video_apply();
}

const char *rib_video_chosen(void)
{
   return chosen_known ? chosen_preset : video_shader_get_current_shader_preset();
}

void rib_video_update(void)
{
   video_shader_ctx_t running = {0};
   if (rib_video_pass_needed() != running_with_pass)
   {
      rib_video_apply();
      return;
   }
   if (!running_with_pass)
      return;
   rib_video_write();
   if (video_shader_driver_get_current_shader(&running) && running.data)
      rib_video_set_values(running.data);
}
