/*
Copyright (C) 2017 by Artyom Sabadyr <zvenayte@gmail.com>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <obs-module.h>
#include <graphics/vec2.h>
#include <graphics/image-file.h>
#include <stdio.h>
#include "obs.h"
#include "obs-internal.h"

OBS_DECLARE_MODULE()

struct obs_source_info stitch_filter;

bool obs_module_load(void)
{
	obs_register_source(&stitch_filter);
	return true;
}

static void stitch_filter_update(void *data, obs_data_t *settings);

struct stitch_filter_data {
	obs_source_t                   *context;

	gs_effect_t                    *effect;
	gs_eparam_t                    *param_alpha;
	//gs_eparam_t                    *param_resO;
	gs_eparam_t                    *param_resI;
	gs_eparam_t                    *param_yrp;
	gs_eparam_t                    *param_ppr;
	gs_eparam_t                    *param_abc;
	gs_eparam_t                    *param_de;
	gs_eparam_t                    *param_crop_c;
	gs_eparam_t                    *param_crop_r;

	gs_texture_t                   *target;
	gs_image_file_t                alpha;
	struct vec2                    resO;
	struct vec2                    resI;
	struct vec3                    yrp;
	float		                   ppr;
	struct vec3                    abc;
	struct vec2                    de;
	struct vec2                    crop_c;
	struct vec2                    crop_r;
};

static const char *stitch_filter_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return "Prosense stitcher";
}

static void *stitch_filter_create(obs_data_t *settings, obs_source_t *context)
{
	struct stitch_filter_data *filter = bzalloc(sizeof(*filter));

	filter->context = context;
	
	stitch_filter_update(filter, settings);
	return filter;
}

static void stitch_filter_destroy(void *data)
{
	struct stitch_filter_data *filter = data;

	obs_enter_graphics();
	gs_effect_destroy(filter->effect);
	obs_leave_graphics();

	bfree(filter);
}

float parse_script(char* str, char* p)
{
	char* num = strstr(str, p);
	if (num != NULL)
	{
		return (float)strtod(num + strlen(p), NULL);
	}
	else
	{
		return 0.0;
	}
}

void parse_script_crop(char* str, struct vec2* crop_c, struct vec2* crop_r, char crop_type)
{
	struct vec4 crop;
	char* num = strchr(str, crop_type);
	if (num != NULL)
	{
		crop.x = strtol(num + 1, &num, 10);
	}
	else return;
	if (num != NULL)
	{
		crop.y = strtol(num + 1, &num, 10);
	}
	else return;
	if (num != NULL)
	{
		crop.z = strtol(num + 1, &num, 10);
	}
	else return;
	if (num != NULL)
	{
		crop.w = strtol(num + 1, NULL, 10);
	}
	else return;
	crop_c->x = (crop.y + crop.x) / 2.0f;
	crop_c->y = (crop.w + crop.z) / 2.0f;

	crop_r->x = (crop.y - crop.x) / 2.0f;
	crop_r->y = (crop.w - crop.z) / 2.0f;
}

void parse_file(char* path, int cam, struct stitch_filter_data* filter)
{
	if (cam < 0)return;
	FILE* pFile;
	char* str;
	str = malloc(150 * 1000 * 1000);

	pFile = fopen(path, "r");
	if (pFile != NULL)
	{
		char* fext = strrchr(path, '.');
		if (strcmp(fext, ".pts") == 0)
		{
			char *effect_path = obs_module_file("pts-stitcher.effect");
			obs_enter_graphics();
			filter->effect = gs_effect_create_from_file(effect_path, NULL);
			obs_leave_graphics();
			bfree(effect_path);

			filter->param_alpha = gs_effect_get_param_by_name(filter->effect, "target");
			//filter->param_resO		= gs_effect_get_param_by_name(filter->effect, "resO");
			filter->param_resI = gs_effect_get_param_by_name(filter->effect, "resI");
			filter->param_yrp = gs_effect_get_param_by_name(filter->effect, "yrp");
			filter->param_ppr = gs_effect_get_param_by_name(filter->effect, "ppr");
			filter->param_abc = gs_effect_get_param_by_name(filter->effect, "abc");
			filter->param_de = gs_effect_get_param_by_name(filter->effect, "de");
			filter->param_crop_c = gs_effect_get_param_by_name(filter->effect, "crop_c");
			filter->param_crop_r = gs_effect_get_param_by_name(filter->effect, "crop_r");

			while (str[0] != 'o')
			{
				if (fgets(str, 150 * 1000 * 1000, pFile) == NULL) break;
			}

			float v = parse_script(str, " v") * M_PI / 180.0f;
			filter->abc.x = parse_script(str, " a");
			filter->abc.y = parse_script(str, " b");
			filter->abc.z = parse_script(str, " c");
			filter->de.x = parse_script(str, " d");
			filter->de.y = parse_script(str, " e");

			int i = -1;
			while (i < cam)
			{
				fgets(str, 150 * 1000 * 1000, pFile);
				while (str[0] != 'o')
				{
					if (fgets(str, 150 * 1000 * 1000, pFile) == NULL)
					{
						break;
					}
				}
				i++;
			}

			filter->yrp.x = parse_script(str, " y") * M_PI / 180.0f;
			filter->yrp.y = parse_script(str, " r") * M_PI / 180.0f;
			filter->yrp.z = parse_script(str, " p") * M_PI / 180.0f;
			parse_script_crop(str, &filter->crop_c, &filter->crop_r, 'C');
			filter->ppr = (filter->crop_r.x + filter->crop_r.y) / v;
		}
		if (strcmp(fext, ".pto") == 0)
		{
			char *effect_path = obs_module_file("pto-stitcher.effect");
			obs_enter_graphics();
			filter->effect = gs_effect_create_from_file(effect_path, NULL);
			obs_leave_graphics();
			bfree(effect_path);

			filter->param_alpha = gs_effect_get_param_by_name(filter->effect, "target");
			//filter->param_resO		= gs_effect_get_param_by_name(filter->effect, "resO");
			filter->param_resI = gs_effect_get_param_by_name(filter->effect, "resI");
			filter->param_yrp = gs_effect_get_param_by_name(filter->effect, "yrp");
			filter->param_ppr = gs_effect_get_param_by_name(filter->effect, "ppr");
			filter->param_abc = gs_effect_get_param_by_name(filter->effect, "abc");
			filter->param_de = gs_effect_get_param_by_name(filter->effect, "de");
			filter->param_crop_c = gs_effect_get_param_by_name(filter->effect, "crop_c");
			filter->param_crop_r = gs_effect_get_param_by_name(filter->effect, "crop_r");

			int i = 0;
			while (i <= cam)
			{
				fgets(str, 150 * 1000 * 1000, pFile);
				while (str[0] != 'i')
				{
					if (fgets(str, 150 * 1000 * 1000, pFile) == NULL)
					{
						break;
					}
				}
				i++;
			}

			float v = parse_script(str, " v") * M_PI / 180.0f;
			filter->abc.x = parse_script(str, " a");
			filter->abc.y = parse_script(str, " b");
			filter->abc.z = parse_script(str, " c");
			filter->de.x = parse_script(str, " d");
			filter->de.y = parse_script(str, " e");

			filter->yrp.x = parse_script(str, " y") * M_PI / 180.0f;
			filter->yrp.y = parse_script(str, " r") * M_PI / 180.0f;
			filter->yrp.z = parse_script(str, " p") * M_PI / 180.0f;
			filter->ppr = parse_script(str, " w") / v;
		}

		fclose(pFile);
	}
	free(str);
}

static void stitch_filter_update(void *data, obs_data_t *settings)
{
	struct stitch_filter_data *filter = data;

	filter->resO.x = (float)4096;
	filter->resO.y = (float)2048;

	int cam = (int)obs_data_get_int(settings, "cam");
	const char *path = obs_data_get_string(settings, "alpha");
	char* res = obs_data_get_string(settings, "res");
	long r = strtol(res, &res, 10);
	if (r > 0)
	{
		filter->resO.x = (float)r;
		if (res != NULL)
		{
			r = strtol(res + 1, NULL, 10);
			if (r > 0)
			{
				filter->resO.y = (float)r;
			}
		}
	}

	gs_image_file_init(&filter->alpha, path);

	obs_enter_graphics();

	gs_image_file_init_texture(&filter->alpha);

	filter->target = filter->alpha.texture;
	obs_leave_graphics();

	path = obs_data_get_string(settings, "project");
	if (path[0] != '\0')
	{
		parse_file(path, cam, filter);
	}
}

static obs_properties_t *stitch_filter_properties(void *data)
{
	obs_properties_t *props = obs_properties_create();

	obs_properties_add_path(props, "alpha", "Mask Image", OBS_PATH_FILE, "*.png", NULL);
	obs_properties_add_path(props, "project", "Project File", OBS_PATH_FILE, "PtGUI/Hugin project (*.pts *.pto)", NULL);
	obs_properties_add_int(props, "cam", "Input #", 0, 99, 1);
	obs_properties_add_text(props, "res", "Resolution", OBS_TEXT_DEFAULT);

	UNUSED_PARAMETER(data);
	return props;
}

static void stitch_filter_defaults(obs_data_t *settings)
{
	obs_data_set_default_int(settings, "cam", 0);
	obs_data_set_default_string(settings, "res", "4096x2048");
}

static void stitch_filter_tick(void *data, float seconds)
{
	struct stitch_filter_data *filter = data;

	obs_source_t *target;
	target = obs_filter_get_target(filter->context);
	filter->resI.x = (float)obs_source_get_base_width(target);
	filter->resI.y = (float)obs_source_get_base_height(target);

	UNUSED_PARAMETER(seconds);
}

static void stitch_filter_render(void *data, gs_effect_t *effect)
{
	struct stitch_filter_data *filter = data;

	if (!filter->target || !filter->effect) {
		obs_source_skip_video_filter(filter->context);
		return;
	}

	if (!obs_source_process_filter_begin(filter->context, GS_RGBA,
		OBS_NO_DIRECT_RENDERING))
		return;

	gs_effect_set_texture(filter->param_alpha, filter->target);
	//gs_effect_set_vec2(filter->param_resO, &filter->resO);
	gs_effect_set_vec2(filter->param_resI, &filter->resI);
	gs_effect_set_vec3(filter->param_yrp, &filter->yrp);
	gs_effect_set_float(filter->param_ppr, filter->ppr);
	gs_effect_set_vec3(filter->param_abc, &filter->abc);
	gs_effect_set_vec2(filter->param_de, &filter->de);
	gs_effect_set_vec2(filter->param_crop_c, &filter->crop_c);
	gs_effect_set_vec2(filter->param_crop_r, &filter->crop_r);

	obs_source_process_filter_end(filter->context, filter->effect, (uint32_t)filter->resO.x, (uint32_t)filter->resO.y);

	UNUSED_PARAMETER(effect);
}

static uint32_t stitch_filter_width(void *data)
{
	if (data != NULL)
	{
		struct stitch_filter_data *filter = data;
		return (uint32_t)filter->resO.x;
	}
	else
	{
		return (uint32_t)4096;;
	}
}

static uint32_t stitch_filter_height(void *data)
{
	if (data != NULL)
	{
		struct stitch_filter_data *filter = data;
		return (uint32_t)filter->resO.y;
	}
	else
	{
		return (uint32_t)2048;
	}
}

struct obs_source_info stitch_filter = {
	.id = "prosense_obs_stitcher_filter",
	.type = OBS_SOURCE_TYPE_FILTER,
	.output_flags = OBS_SOURCE_VIDEO,
	.get_name = stitch_filter_get_name,
	.create = stitch_filter_create,
	.destroy = stitch_filter_destroy,
	.update = stitch_filter_update,
	.get_properties = stitch_filter_properties,
	.get_defaults = stitch_filter_defaults,
	.video_tick = stitch_filter_tick,
	.video_render = stitch_filter_render,
	.get_width = stitch_filter_width,
	.get_height = stitch_filter_height
};
