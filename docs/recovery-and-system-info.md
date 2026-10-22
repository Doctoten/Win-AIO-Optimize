# Recovery, appearance and system information

## Recovery before configuration changes

Registry backup and restore-point options belong to **Windows Setup**. They run only before applying or restoring Windows settings on that page, including optional Cortana/Copilot removal included in those choices. DNS, TCP/IP reset, WiFi import and the standalone optional-app removal page retain their own consent dialogs without these recovery options.

The top of Windows Setup presents **Create a registry backup (Recommended)** and **Create a system restore point (Recommended)** as two separate, initially checked options. Toggling them does not start a backup. Optimization choices themselves remain unchecked until selected. **Review & apply** lists the changes and chosen recovery steps; Cancel remains the default action. Nothing runs until **I agree** is pressed.

The selected recovery steps run on a worker thread while the dialog displays progress. Registry backup precedes the restore-point request. If either selected step fails, the app does not start the requested configuration change. The dialog reports the error and allows retry or cancellation. Cancel returns to Windows Setup, where the user can explicitly adjust recovery options. A failed attempt can leave completed or partial backup files; their path is logged. Registry exports require a volume that supports persistent file permissions.

Registry backup uses [RegSaveKeyExW](https://learn.microsoft.com/en-us/windows/win32/api/winreg/nf-winreg-regsavekeyexw) to save **HKLM\SYSTEM, HKLM\SOFTWARE and the running account's HKCU** as binary `.hiv` files. They are stored in a unique `Registry-{GUID}` directory under `%LOCALAPPDATA%\WinAIOOptimize`, with access restricted to the owner, administrators and SYSTEM. `BACKUP-COMPLETE.txt` is written only when all three exports succeed. Backup privilege is restored after export.

This is a backup of the named registry hives, not a full system-state backup: it excludes other users, SAM/SECURITY, volatile keys and personal files. Binary hives cannot be double-clicked like `.reg` exports. Prefer **Restore last settings** for changes made by the optimizer. Whole-hive recovery requires an experienced administrator, normally in Windows Recovery Environment; the app deliberately does not replace live system hives.

Restore points use [SRSetRestorePointW](https://learn.microsoft.com/en-us/windows/win32/api/srrestoreptapi/nf-srrestoreptapi-srsetrestorepointw) through a dynamically loaded system `srclient.dll`, with paired begin/end calls to complete a checkpoint before applying the requested configuration. [COM callback security](https://learn.microsoft.com/en-us/windows/win32/sr/using-system-restore) is initialized for the local Windows services involved. System Protection must already be enabled. The app exposes a shortcut to Windows System Protection; it does not silently enable the feature or change restore-point frequency policy.

Windows may reuse a recent point under its creation-frequency policy (24 hours by default). A successful request is logged as a Windows-confirmed sequence number, not an unconditional claim that a brand-new point was created. Disabled System Protection, missing components, insufficient space and API failures block the selected changes until the user resolves the problem or changes their selection. Restore points do not replace personal-file backups or guarantee recovery of removed app data.

## Light and dark appearance

Feature pages and change reviews are embedded in one main window. Dashboard/Back or Escape returns home without destroying page selections. The title-bar Close button exits the application when no job or review is active. Windows-owned file pickers and system settings still use Windows dialogs.

Pages use the same centered content width as the dashboard. Layout computes card backgrounds before moving children and invalidates the complete page after placement. Report headers and bodies are placed explicitly together; native scrollbar and column updates cannot repeatedly shrink or shift the report. Headers own the top corners and report bodies join them without a second rounded seam.

The default **Windows theme** option follows `AppsUseLightTheme`. **Light** and **Dark** allow an app-only override stored in the app's HKCU preference key. Theme changes repaint the dashboard, app dialogs, buttons, list headers, lists, edits and selectors without rebuilding controls or clearing their data. Windows setting changes are handled while the app is open. High Contrast uses the user's system colors instead of the custom palette.

The app requests the documented [DWM dark title bar and rounded corners](https://learn.microsoft.com/en-us/windows/win32/api/dwmapi/ne-dwmapi-dwmwindowattribute) on Windows 11. Windows-owned file pickers, message boxes and some native chrome may retain the system's appearance.

Windows Setup uses grouped cards for Privacy, AI, Start & Search, System, Taskbar and File Explorer. Search matches both English and Vietnamese labels and group names. Resizing reflows the cards; scrolling and keyboard focus keep settings reachable. Unsupported settings are dimmed and expose their reason in the detail area. Changing language preserves the search, settings and recovery selections. Native control surfaces are repainted in full after label changes to avoid leftover white areas.

## System information

The dashboard shows Windows, CPU and installed RAM. Account/build diagnostics and the activity log are hidden behind **Advanced**. **System information** initially shows essential Windows, CPU, RAM, graphics and storage properties. Its **Advanced** button reveals the full categorized inventory, full-value view and explicit **Copy row** action. Refresh runs in a background worker; data represents the most recent refresh, not continuous monitoring.

Read-only WMI queries cover the computer/model/account, Windows version/build/install/boot times, CPU cores/threads/clock/virtualization, individual RAM modules, GPU/driver/display mode, physical disks, local volumes/free space, motherboard, BIOS, IP-enabled adapters/DNS/DHCP and battery. Native APIs add firmware mode and reported Secure Boot state. [DXGI adapter descriptions](https://learn.microsoft.com/en-us/windows/win32/api/dxgi/ns-dxgi-dxgi_adapter_desc1) provide dedicated/shared video-memory sizes without the 32-bit WMI `AdapterRAM` limitation.

Missing devices and properties are shown as unavailable. Provider failures are reported with an error code and partial results remain visible. Enumeration has bounded row counts and waits. Driver/firmware values can be incomplete; disk status is Windows-reported status, not a SMART health diagnosis. Dates and numeric provider enums are explicitly labeled with their format. The inventory does not collect passwords or product keys, and hardware identifiers are not written to the activity log or sent over the network.

## Tiếng Việt

Hai lựa chọn **Tạo bản sao Registry (Khuyên dùng)** và **Tạo điểm khôi phục hệ thống (Khuyên dùng)** nằm riêng ở đầu **Windows Setup**, được đánh dấu sẵn. Chỉ chạy khi xác nhận áp dụng/khôi phục cài đặt tại đây; đánh dấu lựa chọn không tự tạo backup. Các mục tối ưu vẫn mặc định bỏ chọn. Trang DNS, WiFi và gỡ ứng dụng riêng giữ xác nhận của chúng, không kèm hai lựa chọn bảo vệ. Nếu bước bảo vệ thất bại, ứng dụng dừng trước thay đổi; bạn có thể thử lại hoặc hủy để điều chỉnh lựa chọn trong Windows Setup.

Registry được lưu dưới dạng `.hiv` cho SYSTEM, SOFTWARE và HKCU của tài khoản chạy app, trong thư mục `Registry-{GUID}` riêng tại `%LOCALAPPDATA%\WinAIOOptimize`. File `BACKUP-COMPLETE.txt` đánh dấu ba phần đã được xuất thành công. Đây không phải bản sao toàn bộ trạng thái máy hoặc dữ liệu cá nhân. Khôi phục nguyên hive cần người có chuyên môn, thường qua Windows Recovery Environment; với các tối ưu của app, ưu tiên **Khôi phục cài đặt**.

System Protection phải được bật từ trước. Nút **Bảo vệ hệ thống…** mở thiết lập Windows. App không tự bật bảo vệ hoặc thay đổi giới hạn tạo điểm khôi phục. Windows có thể dùng lại điểm gần đây theo chính sách tần suất; nhật ký ghi số thứ tự được Windows xác nhận. Điểm khôi phục không thay thế sao lưu file cá nhân hoặc bảo đảm lấy lại dữ liệu ứng dụng đã gỡ.

Giao diện mặc định **Theo Windows**, hỗ trợ chọn riêng **Sáng** hoặc **Tối**, giữ nguyên dữ liệu nhập khi đổi. Windows Setup chia cài đặt thành các nhóm, có tìm kiếm Anh/Việt, cuộn và bố cục thích ứng kích thước cửa sổ. Chuyển ngôn ngữ giữ nguyên tìm kiếm, mục tối ưu và lựa chọn bảo vệ; toàn bộ bề mặt nút được vẽ lại để tránh khoảng trắng sót lại. Chế độ tương phản cao sử dụng màu hệ thống. Hộp thoại do Windows quản lý có thể giữ giao diện của hệ điều hành.

Các trang chức năng và phần xác nhận nằm trong một cửa sổ. Nhấn Tổng quan hoặc Escape để quay lại; lựa chọn được giữ nguyên. Thông tin máy mặc định chỉ hiện các thông số thiết yếu. Mở **Nâng cao** để xem bộ lọc nhóm, sao chép dòng và toàn bộ máy/model, Windows, CPU, từng thanh RAM, GPU/VRAM/driver, ổ đĩa, mainboard, BIOS, mạng/DNS, pin, UEFI và Secure Boot. Dữ liệu là trạng thái tại lần làm mới. Mục không có hoặc không đọc được sẽ được ghi rõ; thông tin do driver cung cấp có thể không đầy đủ. Không thu thập mật khẩu/key sản phẩm và không tự ghi định danh phần cứng vào nhật ký hoặc gửi ra mạng.
