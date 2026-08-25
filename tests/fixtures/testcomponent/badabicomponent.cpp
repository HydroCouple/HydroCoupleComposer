/*!
 * \file   badabicomponent.cpp
 * \brief  Exports the component entry points but reports a foreign ABI stamp.
 *
 * Proves the guard in ComponentLibrary::load: the info entry point here would
 * return a garbage pointer if it were ever called, so a passing test is also
 * evidence that the loader stopped at the stamp check and never invoked it.
 */

#include "plugins/componentabi.h"

extern "C" HYDROCOUPLE_COMPONENT_EXPORT const char *
hydrocouple_component_abi_v1(void)
{
  return "hc-abi/1;iface=99;cxx=nonesuch-0;stdlib=nonesuch;bits=8";
}

extern "C" HYDROCOUPLE_COMPONENT_EXPORT HydroCouple::IComponentInfo *
hydrocouple_component_info_v1(void)
{
  // Never reached if the loader honours its own contract.
  return reinterpret_cast<HydroCouple::IComponentInfo *>(0x1);
}
