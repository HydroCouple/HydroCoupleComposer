/*!
 * \file   badabicomponent.cpp
 * \brief  Exports the component entry points but reports a foreign ABI stamp.
 *
 * Proves the guard in ComponentLibrary::load: the info entry point here would
 * return a garbage pointer if it were ever called, so a passing test is also
 * evidence that the loader stopped at the stamp check and never invoked it.
 */

#include "plugins/componentabi.h"
#include <string>

extern "C" HYDROCOUPLE_COMPONENT_EXPORT const char *
hydrocouple_component_abi_v1(void)
{
  static const std::string oldInterface = [] {
    std::string stamp = HYDROCOUPLE_COMPONENT_ABI_STAMP;
    const auto begin = stamp.find(";iface=");
    const auto end = stamp.find(';', begin + 1);
    stamp.replace(begin, end - begin, ";iface=2");
    return stamp;
  }();
  return oldInterface.c_str();
}

extern "C" HYDROCOUPLE_COMPONENT_EXPORT HydroCouple::IComponentInfo *
hydrocouple_component_info_v1(void)
{
  // Never reached if the loader honours its own contract.
  return reinterpret_cast<HydroCouple::IComponentInfo *>(0x1);
}
