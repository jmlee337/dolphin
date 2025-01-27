// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.input.model;

import org.dolphinemu.dolphinemu.features.input.model.controlleremu.NumericSetting;
import org.dolphinemu.dolphinemu.features.settings.model.AbstractBooleanSetting;
import org.dolphinemu.dolphinemu.features.settings.model.Settings;

public class InputMappingBooleanSetting implements AbstractBooleanSetting
{
  private final NumericSetting mNumericSetting;

  public InputMappingBooleanSetting(NumericSetting numericSetting)
  {
    mNumericSetting = numericSetting;
  }

  @Override
  public boolean getBoolean(Settings settings)
  {
    return mNumericSetting.getBooleanValue();
  }

  @Override
  public void setBoolean(Settings settings, boolean newValue)
  {
    mNumericSetting.setBooleanValue(newValue);
  }

  @Override
  public boolean isOverridden(Settings settings)
  {
    return false;
  }

  @Override
  public boolean isRuntimeEditable()
  {
    return true;
  }

  @Override
  public boolean delete(Settings settings)
  {
    mNumericSetting.setBooleanValue(mNumericSetting.getBooleanDefaultValue());
    return true;
  }
}
