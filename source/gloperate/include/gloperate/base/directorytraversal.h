#pragma once

#include <string>

#include <gloperate/gloperate_api.h>


namespace gloperate
{

GLOPERATE_API void scanDirectory(
    const std::string & directory, 
    const std::string & fileExtension, 
    bool recursive=false);

GLOPERATE_API void scanDirectory(
    std::string baseDirectory,
    std::string includeSubdirectory,
    const std::string & fileExtension,
    bool recursive = false);

} // namespace globjectsutils
