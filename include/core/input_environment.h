#pragma once

namespace inputenvironment {

void SetEnabled(bool enabled);
bool IsEnabled();
bool IsInputActive();
bool ShouldPauseAutomation();
bool ShouldSuppressScriptKey(unsigned int virtualKey);
void ForceNonInputState();
void Shutdown();

} // namespace inputenvironment
