/*******************************************************************************
 BEGIN_JUCE_MODULE_DECLARATION

  ID:                 sbaudio_common
  vendor:             SB Audio
  version:            1.0.0
  name:               SB Audio common
  description:        Felles kode for SB Audio-plugins (farger, look & feel, presets).
  license:            Proprietary
  minimumCppStandard: 17

  dependencies:       juce_gui_basics juce_audio_processors

 END_JUCE_MODULE_DECLARATION
*******************************************************************************/

#pragma once
#define SBAUDIO_COMMON_H_INCLUDED

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include "colours/sbaudio_Colours.h"
#include "gui/sbaudio_LookAndFeel.h"
#include "gui/sbaudio_Brand.h"
#include "gui/sbaudio_Knob.h"
#include "dsp/sbaudio_AnalyzerRing.h"
#include "dsp/sbaudio_ReverbEngine.h"
#include "presets/sbaudio_PresetManager.h"
#include "gui/sbaudio_PresetBar.h"
