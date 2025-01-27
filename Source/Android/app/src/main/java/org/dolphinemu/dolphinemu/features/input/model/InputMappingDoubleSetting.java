// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.input.model;

import org.dolphinemu.dolphinemu.features.input.model.controlleremu.NumericSetting;
import org.dolphinemu.dolphinemu.features.settings.model.AbstractFloatSetting;
import org.dolphinemu.dolphinemu.features.settings.model.Settings;

// Yes, floats are not the same thing as doubles... They're close enough, though
public class InputMappingDoubleSetting implements AbstractFloatSetting
{
  private final NumericSetting mNumericSetting;

  public InputMappingDoubleSetting(NumericSetting numericSetting)
  {
    mNumericSetting = numericSetting;
  }

  @Override
  public float getFloat(Settings settings)
  {
    return (float) mNumericSetting.getDoubleValue();
  }

  @Override
  public void setFloat(Settings settings, float newValue)
  {
    mNumericSetting.setDoubleValue(newValue);
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
    mNumericSetting.setDoubleValue(mNumericSetting.getDoubleDefaultValue());
    return true;
  }
}
