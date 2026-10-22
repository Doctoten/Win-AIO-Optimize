# Windows Setup implementation and compatibility

The optimizer runs in C. No remote script, PowerShell process, shell command string, or downloaded optimization payload is used. Registry operations use the 64-bit view, services use the Service Control Manager, power settings use PowrProf, and optional app removal uses the Windows Runtime PackageManager ABI.

## Consent and recovery

Every row starts unchecked. Unsupported rows cannot be checked. Applying opens a scrollable review with Cancel as the default; only explicit acceptance starts the worker. Removal lists exact package identities. Recall deletion, Store cache permissions, standby connectivity, and app data loss have additional explanations in the review.

Before changing settings, the application captures original values (including whether they existed), service startup configuration, power values, and selected Store cache permissions. It flushes the snapshot to disk and atomically replaces `%LOCALAPPDATA%\WinAIOOptimize\optimizer-last.snapshot`. A capture/save failure stops the run before settings or packages change. Each run replaces the previous snapshot, so **Restore last settings undoes only the most recent settings batch**, not all historical runs. A failure during application can leave a partial change; the snapshot remains available and the log records each result.

Restore validates the snapshot version, record identities, uniqueness, sizes and value formats. Registry paths are chosen from the compiled catalog, never supplied by the snapshot. Store ACLs are serialized as bounded SDDL text and restored only if the file identity still matches. Absent registry values are removed on restore; empty parent keys may remain. Restoring does not reinstall packages, recover deleted app data or Recall snapshots, or override domain management.

## Policy coverage

| Group | Implementation and limits |
| --- | --- |
| Diagnostics | `AllowTelemetry=0` on Enterprise/Education and `1` elsewhere; feedback prompts are controlled where supported. This limits Windows diagnostic data, not every Microsoft connection. Required diagnostic data remains on Home/Pro. See [Microsoft System policy](https://learn.microsoft.com/en-us/windows/client-management/mdm/policy-csp-system#allowtelemetry). |
| Activity/tracking/ads | Activity feed/publishing/upload policies on supported editions; current-user app tracking, advertising ID and tailored-experience preferences. Existing activity data is preserved. See [Microsoft Privacy policy](https://learn.microsoft.com/en-us/windows/client-management/mdm/policy-csp-privacy). |
| Windows suggestions | Consumer/tip policies plus current-user ContentDeliveryManager, Explorer and onboarding preferences. Some policies are Enterprise/Education-only; preference keys are version-sensitive and may be overwritten by Windows. |
| Edge | New-tab content, recommendations, personalization, rewards and shopping policies. Restart Edge and inspect `edge://policy` for the installed browser's actual policy status. Browser versions and managed profiles can change applicability. See [NewTabPageContentEnabled](https://learn.microsoft.com/en-us/deployedge/microsoft-edge-policies/NewTabPageContentEnabled). |
| Bing/Cortana | Search suggestion/web/highlight policies and compatibility preferences; Cortana app removal for the current account. Windows Search itself is preserved. Edition restrictions are logged. See [Microsoft Search policy](https://learn.microsoft.com/en-us/windows/client-management/mdm/policy-csp-search). |
| Copilot | Legacy Windows Copilot policy/button plus removal of `Microsoft.Copilot_8wekyb3d8bbwe` for the current account. This does not uninstall every AI component in Edge, Microsoft 365, or Windows. |
| Recall | Windows 11 24H2, supported update/edition and installed ADMX required. `AllowRecallEnablement=0` and `DisableAIDataAnalysis=1`. Windows can delete saved Recall snapshots; consent explicitly says this is irreversible. See [WindowsAI policies](https://learn.microsoft.com/en-us/windows/client-management/mdm/policy-csp-windowsai#allowrecallenablement). |
| Click to Do | `DisableClickToDo=1`, only when the installed WindowsCopilot ADMX advertises it and the edition supports it. The policy may be preview-only on a particular build. See [DisableClickToDo](https://learn.microsoft.com/en-us/windows/client-management/mdm/policy-csp-windowsai#disableclicktodo). |
| AI Fabric service | Only an existing `WSAIFabricSvc` is changed: Automatic becomes Manual, Disabled remains Disabled. It is not forcibly stopped; Windows may still start a Manual service on demand. |
| Widgets | Taskbar preference and supported News/Widgets policy. The separate lock-screen row requires the installed `DisableWidgetsOnLockScreen` policy; it is not universally available on stable builds. See [NewsAndInterests policies](https://learn.microsoft.com/en-us/windows/client-management/mdm/policy-csp-newsandinterests). |
| Meet Now / 3D Objects | Windows 10 only. Hide the icon / known-folder entry; no files are deleted. |
| Drag Tray | Windows 11 only. `HKCU\Software\Microsoft\Windows\CurrentVersion\CDP\DragTrayEnabled=0`, gated on an existing preference or supported tray-update build. This is a version-sensitive preference, not a universal Windows policy. |
| Extensions | Set the current user's Explorer `HideFileExt=0`. |
| Modern Standby | Windows 11 only, S0 low-power idle and readable standby-connectivity setting required. Disable AC/DC connectivity for the active scheme; save both values and its GUID. Restore does not switch the active scheme. Sleep notifications/remote access can stop. [Power capabilities](https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-system_power_capabilities) describe the S0 detection. |

## Store suggestions compatibility option

There is no Microsoft-documented universal policy used here. The option blocks read/write data access to the existing current-user `%LOCALAPPDATA%\Packages\Microsoft.WindowsStore_8wekyb3d8bbwe\LocalState\store.db` cache, following the cache-blocking approach documented in [Win11Debloat's Store suggestions implementation](https://github.com/Raphire/Win11Debloat/blob/master/Scripts/Features/Set-StoreSearchSuggestions.ps1). This program independently implements it in C; it never downloads or executes that script.

The UI calls this a compatibility workaround. It may affect recommendations inside Microsoft Store and may require closing/reopening Store or signing out. Missing caches are skipped; the app does not create/delete a database or take ownership. Reparse points and hard links are rejected, original DACL/protection are saved, and metadata permission is retained so restoration is possible. Windows can replace the cache later; a changed file identity prevents an unsafe restore and is reported in the log. Verify actual Search behavior in a VM before broad deployment.

## Optional packages

The bloatware screen enumerates a compiled allowlist of optional package families for the effective Windows account. It excludes Windows Store, shell components, frameworks, servicing and security components. It calls `RemovePackageAsync` for exact confirmed full names and checks asynchronous/extended errors. It does not deprovision applications for future accounts or remove them from other users. See [PackageManager.RemovePackageAsync](https://learn.microsoft.com/en-us/uwp/api/windows.management.deployment.packagemanager.removepackageasync).

## Verification scope

Self-tests cover catalog uniqueness, restricted package families, WinRT activation without deployment, malformed snapshot records, bounded SDDL and DNS/timer validation. Building and successful registry writes do not prove a Windows edition honors a policy. See the repository's [VM acceptance matrix](../../TESTING.md). Actual optimizations/removals have not been applied to the developer machine as part of implementation.
