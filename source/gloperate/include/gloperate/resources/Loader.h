#pragma once


#include <functional>

#include <gloperate/resources/AbstractLoader.h>


namespace gloperate
{

/**
*  @brief
*    Loader class template
*/
template <typename T>
class Loader : public AbstractLoader 
{
public:
    /**
    *  @brief
    *    Constructor
    */
    Loader();

    /**
    *  @brief
    *    Destructor
    */
    virtual ~Loader();

    /**
    *  @brief
    *    Load resource from file
    *
    *  @param[in] filename
    *    File name
    *  @param[in] progress
    *    Callback function that is invoked on progress (can be empty)
    *
    *  @return
    *    Loaded resource (can be null)
    */
    virtual T * load(const std::string & filename, std::function<void(int, int)> progress) const = 0;
};

} // namespace gloperate


#include <gloperate/resources/Loader.hpp>
