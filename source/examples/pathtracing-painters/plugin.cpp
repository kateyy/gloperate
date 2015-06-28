#include <gloperate/plugin/plugin_api.h>

#include "pathtracing/PathTracingPainter.h"


GLOPERATE_PLUGIN_LIBRARY

    GLOPERATE_PLUGIN(PathTracingPainter
    , "PathTracingPipeline"
    , "Displays a path tracing demo scene using a pipeline"
    , "gloperate team"
    , "v1.0.0" )

GLOPERATE_PLUGIN_LIBRARY_END
