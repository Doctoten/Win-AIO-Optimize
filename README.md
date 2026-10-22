# Win AIO Optimize

<img src="assets/logo.svg" alt="Win AIO Optimize" width="80" height="80">

![Version](https://img.shields.io/badge/version-6.0.1-blue?style=for-the-badge)
![Windows](https://img.shields.io/badge/Windows-10%20%2F%2011%20x64-0078D6?style=for-the-badge)
![Native C](https://img.shields.io/badge/native-C11%20%2B%20Win32-555555?style=for-the-badge)
![Portable](https://img.shields.io/badge/portable-EXE-2D9F2D?style=for-the-badge)
[![MIT License](https://img.shields.io/badge/license-MIT-green?style=for-the-badge)](LICENSE)

**[English](#english) | [Tiếng Việt](#tieng-viet)**

[Releases / Bản phát hành](https://github.com/Doctoten/Windows-utility-tool/releases) · [Report a bug / Báo lỗi](https://github.com/Doctoten/Windows-utility-tool/issues) · [Technical notes / Ghi chú kỹ thuật](docs/optimizer-notes.md)

<a id="english"></a>

## English

**Win AIO Optimize** brings Windows optimization, optional-app removal, DNS configuration, backups, and a shutdown timer into one native desktop app. It runs directly from a portable EXE on Windows 10/11 x64, with no installation, Python, or additional runtime to set up.

English and Vietnamese are included in the app. Choose the changes you want, read their effects, and confirm before applying. The Windows optimizer is written in C and works locally; PowerShell is only used by the optional download helper.

All feature pages and change reviews stay in one main window. Use **Dashboard** or **Escape** to go back. Everyday information is shown first; **Advanced** reveals full specifications, network details and diagnostic logs. Registry backup and restore-point choices remain clearly visible in **Windows Setup**.

<a id="usage-en"></a>

### Usage

> [!IMPORTANT]
> All optimization options start unchecked. Nothing is applied at startup. Every optimization requires your selection and confirmation, and Windows is never restarted automatically.

#### Quick method

**Download & run the script automatically via PowerShell.**

1. Open PowerShell or Windows Terminal.
2. Copy and paste this command into PowerShell:

   ```powershell
   & ([scriptblock]::Create((irm "https://raw.githubusercontent.com/Doctoten/Windows-utility-tool/main/install.ps1")))
   ```

3. Wait for the launcher to download Win AIO Optimize, verify its SHA256 and product version, then open the app.
4. Accept the Windows Administrator prompt. Select your language and carefully review each setting before confirming changes.

**No manual download or installation is needed.** The command retrieves the launcher directly into PowerShell; it does not save a separate `.ps1` file. The launcher automatically downloads the portable EXE and checksum to `%LOCALAPPDATA%\WinAIOOptimize\Downloads`. Internet access is required.

The launcher targets version `6.0.1` and stops if the release, checksum or version check fails. It never applies optimizations itself. SHA256 verifies file integrity; it is not a publisher signature.

> [!NOTE]
> The command requires `install.ps1` on the repository's `main` branch and a published `v6.0.1` release containing `win_aio_optimize.exe` and `win_aio_optimize.exe.sha256`. If these are unavailable, use the build instructions below.

#### Manual method

<details>
<summary>Download and run the portable EXE yourself.</summary>

1. Open [Releases](https://github.com/Doctoten/Windows-utility-tool/releases) and choose a release containing `win_aio_optimize.exe` and its `.sha256` file.
2. Download the EXE and checksum to a folder of your choice. If packaged as a ZIP, extract it first.
3. Compare the checksum with `Get-FileHash .\win_aio_optimize.exe -Algorithm SHA256` in PowerShell from that folder.
4. Double-click `win_aio_optimize.exe` and accept the Administrator prompt from Windows.
5. Select your language, open a feature, and review the proposed changes before confirming.

**The EXE is the app. No installation wizard or batch file is needed to use a release.** A locally built copy is available at `dist\win_aio_optimize.exe`.

</details>

#### Language selection

<details>
<summary>Switch between English and Vietnamese, or choose a language at launch.</summary>

Use the selector in the top-right corner. On first launch, Vietnamese is selected when the Windows UI language is Vietnamese; otherwise, English is used. The app saves your choice for the account running it.

Open pages update immediately while keeping DNS input, selected options, and timer values. Complete or cancel an active task or confirmation before switching.

To override the language for one launch, run from the folder containing the EXE:

```powershell
.\win_aio_optimize.exe --lang=en
.\win_aio_optimize.exe --lang=vi
```

These arguments do not overwrite the saved preference. Both translations are embedded in the EXE. Product names, package identities, system-tool output, and Windows-owned dialog text retain the text supplied by Windows; existing logs keep their original language.

</details>

### Features

Availability depends on Windows edition, build, installed components, and hardware. Unsupported options are identified in the app. See the [optimization notes](docs/optimizer-notes.md) for policy mappings and compatibility details.

#### Dashboard and system information

- A redesigned dashboard with a tool sidebar, hardware overview, recovery shortcuts, timer and activity log.
- Light and Dark modes, following Windows by default, with an optional app-only override and High Contrast support.
- A categorized hardware inventory: CPU, RAM modules, GPU/VRAM, drivers, disks and free space, motherboard, BIOS, Windows, networking, battery, firmware mode and Secure Boot.
- Refresh information in the background, filter components and copy individual values. Missing data and query errors are reported explicitly.

#### Recovery before changes

- **Create a registry backup (Recommended)** — saves SYSTEM, SOFTWARE and the current account's HKCU in a private backup folder.
- **Create a system restore point (Recommended)** — requests a Windows restore point before the selected configuration changes.
- Both recovery options are clearly labeled at the top of **Windows Setup** and start checked. They run only after you confirm applying or restoring Windows settings there. A selected protection failure stops the requested changes.
- Windows Setup groups settings into Privacy, AI, Start & Search, System, Taskbar and File Explorer cards, with search and a layout that adapts to the window size. Optimization choices start unchecked.
- DNS, WiFi and standalone optional-app removal keep their own confirmations without these recovery options. Open backups or Windows System Protection from the sidebar.

System Protection must already be enabled. Windows may reuse a recent restore point under its frequency policy. See [recovery, themes and system information](docs/recovery-and-system-info.md) for backup scope and limitations.

#### Optional-app removal

- Review installed apps from a defined list of optional packages.
- Choose exactly which apps to remove for the current account through Windows PackageManager.
- See the package list and data-loss notice before approving removal.

#### Privacy and advertising

- Reduce telemetry and diagnostic collection, subject to Windows edition limits.
- Turn off activity history, app-launch tracking, and personalized advertising.
- Turn off Windows tips, suggestions, and promotional content.
- Reduce Microsoft Edge advertising, suggestions, and the MSN feed.

#### Search and AI

- Turn off Bing web/Bing AI integration in Windows Search and disable/remove Cortana where supported.
- Block Microsoft Store app suggestions in Search on recognized Windows 11 configurations.
- Disable Copilot and remove its installed packages for the current account.
- Disable Recall and Click to Do on supported Windows 11 builds.
- Change WSAIFabricSvc startup to Manual on supported Windows 11 systems; Windows can still start it on demand.

#### Desktop and File Explorer

- Hide taskbar widgets/news and disable supported lock-screen widgets.
- Hide the Meet Now taskbar icon on Windows 10.
- Turn off the Windows 11 Drag Tray when the matching setting is available.
- Display extensions such as `.exe`, `.txt`, and `.pdf` in File Explorer.
- Hide the 3D Objects entry under This PC on Windows 10 without deleting its files.
- Disable networking during Modern Standby on supported Windows 11 hardware and power configurations.

#### Network and DNS

- View adapters, IP addresses, MAC addresses, and DNS servers.
- Choose Cloudflare, Google, Quad9, custom DNS, or automatic DNS through DHCP.
- Configure IPv4 and IPv6 separately, with the adapter and server addresses shown before confirmation.
- Flush the DNS cache or reset IPv4, IPv6, and Winsock, with results reported for each step.

DNS changes can affect VPNs and corporate networks. Partial success is reported explicitly; a network reset requires a restart.

#### Backups and shutdown timer

- Export WiFi profiles through the Windows WLAN API, with a separate choice for password export.
- Restore selected WiFi XML profiles and report successful and failed imports.
- Export third-party drivers into a separate folder using DISM. Windows inbox drivers are outside the export scope.
- Schedule a shutdown in minutes or hours, view the saved countdown, and cancel the request.
- Run longer operations in the background and record results without logging WiFi passwords.

Keep XML backups containing plaintext WiFi passwords private. Driver exports can take several minutes.

### Restore and compatibility

> [!NOTE]
> Settings restore covers the most recent saved optimization run. It does not reinstall removed apps, recover their data, or recover deleted Recall snapshots.

<details>
<summary>Read about saved settings, Windows limitations, and account scope.</summary>

**Saved settings.** Before writing supported registry, service, power, or permission changes, the app saves their previous state. A new snapshot replaces the previous one. Cancel is the default in the confirmation dialog.

**Windows support.** Some policies require Enterprise/Education or newer builds. Home/Pro may still send required diagnostic data, so zero telemetry is not guaranteed. Organization policies and Windows Update can override settings. Disabling an AI feature does not remove every AI component. Sign out or restart when necessary to refresh interface changes. The UI and log distinguish success, failure, and unsupported operations.

**Store suggestions.** This option blocks data reads/writes to the recognized `store.db` cache and saves its original permissions. It may affect suggestions inside Microsoft Store. This is a compatibility workaround, not an official Microsoft policy.

**Account scope.** HKCU settings, WiFi operations, and app removal use the account shown in the main window. If UAC uses another administrator's credentials, per-user changes apply to that administrator's account.

**Application data.** Portable execution still saves logs and settings snapshots under `%LOCALAPPDATA%\WinAIOOptimize`. The log is `activity.log`; the language preference is stored under `HKCU\Software\Doctoten\WinAIOOptimize`.

</details>

### Build and validation

<details>
<summary>Build the native EXE and run read-only checks.</summary>

Use an x64 toolchain: **LLVM-MinGW / MinGW-w64** with `gcc` and `windres` on PATH, or **Visual Studio 2022 Build Tools** with the Windows SDK and CMake.

From the repository root:

```bat
build.bat
run.bat
```

Output: `dist\win_aio_optimize.exe` and its `.sha256` checksum. `run.bat` builds the app if the EXE is missing.

With CMake and Visual Studio:

```powershell
cmake -S . -B build-native -A x64
cmake --build build-native --config Release
ctest --test-dir build-native -C Release --output-on-failure
```

Output: `build-native/bin/Release/win_aio_optimize.exe` and its `.sha256` checksum. For Ninja/MinGW, omit `-A x64` and select the appropriate generator/toolchain; output is under `build-native/bin/`.

Run the complete read-only validation without elevation or system changes:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\validate.ps1
# Optional: also query this computer's hardware.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\validate.ps1 -Inventory
```

CI treats compiler warnings as errors and runs the same validation script with `-Binary build-native/bin/Release/win_aio_optimize.exe`. It checks UTF-8, PowerShell syntax, version consistency, executable metadata and SHA256, then runs self-tests and all four English/Vietnamese and Light/Dark UI combinations. Tests cover DNS validation, shutdown limits, optimization/package boundaries, recovery failure handling, inventory formatting and language switching without losing input. Logs are saved under `build-native/validation-*` and uploaded by CI. Use `-SourceOnly` to check source files without a built EXE.

Actual system changes need testing in disposable Windows 10/11 VMs with snapshots. See [TESTING.md](TESTING.md) for results and the acceptance checklist. Do not apply optimizations, change DNS, remove apps, or schedule shutdowns just to test on a development machine.

Native source and resources are in `src/`; policy documentation is in `docs/`. Generated `build-native/` and `dist/` folders are excluded from Git. GitHub links retain the repository name `Doctoten/Windows-utility-tool`.

</details>

### Feedback and contributions

Report problems or propose features through [GitHub Issues](https://github.com/Doctoten/Windows-utility-tool/issues). Include the app version, Windows edition/build, steps to reproduce, and relevant error codes. Remove personal information from logs and never attach WiFi passwords or private backup files.

Code and translation improvements are welcome. For system-changing code, document the affected Windows versions, the confirmation shown to users, and validation performed. See [TESTING.md](TESTING.md).

### License

This project is released under the [MIT License](LICENSE).

Developed by **Doctoten**. [Back to usage](#usage-en) · [Tiếng Việt](#tieng-viet)

---

<a id="tieng-viet"></a>

## Tiếng Việt

**Win AIO Optimize** tập hợp tối ưu Windows, gỡ ứng dụng tùy chọn, đổi DNS, sao lưu và hẹn giờ tắt máy trong một ứng dụng desktop native. Chạy trực tiếp bằng EXE portable trên Windows 10/11 x64, không cần cài đặt, Python hay thiết lập thêm môi trường chạy.

Ứng dụng tích hợp tiếng Anh và tiếng Việt. Chọn thay đổi bạn muốn, đọc ảnh hưởng và xác nhận trước khi áp dụng. Bộ tối ưu Windows được viết bằng C và chạy tại máy; PowerShell chỉ dùng cho công cụ tải xuống tùy chọn.

Các trang chức năng và phần xem lại thay đổi nằm trong một cửa sổ chính. Nhấn **Tổng quan** hoặc **Escape** để quay lại. Mặc định chỉ hiển thị thông tin cần dùng; mục **Nâng cao** chứa thông số đầy đủ, chi tiết mạng và nhật ký. Hai lựa chọn sao lưu Registry và tạo điểm khôi phục vẫn hiển thị rõ trong **Windows Setup**.

<a id="usage-vi"></a>

### Cách sử dụng

> [!IMPORTANT]
> Mọi mục tối ưu mặc định bỏ chọn. Không tự áp dụng khi khởi động. Mỗi tối ưu cần người dùng chọn và xác nhận; ứng dụng không tự khởi động lại Windows.

#### Cách nhanh

**Tải và chạy script tự động qua PowerShell.**

1. Mở PowerShell hoặc Windows Terminal.
2. Sao chép và dán lệnh sau vào PowerShell:

   ```powershell
   & ([scriptblock]::Create((irm "https://raw.githubusercontent.com/Doctoten/Windows-utility-tool/main/install.ps1")))
   ```

3. Chờ công cụ tự tải Win AIO Optimize, kiểm tra SHA256 và phiên bản rồi mở ứng dụng.
4. Chấp nhận yêu cầu Administrator của Windows. Chọn ngôn ngữ, đọc kỹ từng cài đặt và xác nhận trước khi thay đổi.

**Không cần tự tải file hay cài đặt.** Lệnh đọc script trực tiếp vào PowerShell, không lưu một file `.ps1` riêng. Script tự tải EXE portable và checksum vào `%LOCALAPPDATA%\WinAIOOptimize\Downloads`. Cách này cần kết nối Internet.

Script mở phiên bản `6.0.1` và dừng nếu thiếu release, checksum hoặc xác minh phiên bản thất bại. Script không tự áp dụng tối ưu. SHA256 kiểm tra tính toàn vẹn của file, không phải chữ ký nhà phát hành.

> [!NOTE]
> Để lệnh hoạt động, nhánh `main` phải có `install.ps1` và release `v6.0.1` đã công bố phải chứa `win_aio_optimize.exe` cùng `win_aio_optimize.exe.sha256`. Nếu chưa có, dùng hướng dẫn build bên dưới.

#### Cách thủ công

<details>
<summary>Tự tải và chạy EXE portable.</summary>

1. Mở [Bản phát hành](https://github.com/Doctoten/Windows-utility-tool/releases), chọn bản có `win_aio_optimize.exe` và file `.sha256` tương ứng.
2. Tải EXE và checksum vào thư mục bạn muốn. Nếu được đóng gói dạng ZIP, giải nén trước.
3. Mở PowerShell tại thư mục đó, chạy `Get-FileHash .\win_aio_optimize.exe -Algorithm SHA256` và đối chiếu checksum.
4. Nhấp đúp `win_aio_optimize.exe`, chấp nhận yêu cầu quyền Administrator của Windows.
5. Chọn ngôn ngữ, mở chức năng cần dùng và xem lại thay đổi trước khi xác nhận.

**EXE chính là ứng dụng. Bản phát hành không cần trình cài đặt hoặc file batch để chạy.** Bản build tại máy nằm ở `dist\win_aio_optimize.exe`.

</details>

#### Chọn ngôn ngữ

<details>
<summary>Đổi giữa tiếng Anh và tiếng Việt, hoặc chọn ngôn ngữ khi mở ứng dụng.</summary>

Dùng ô chọn ở góc phải. Lần đầu, ứng dụng chọn tiếng Việt nếu ngôn ngữ giao diện Windows là tiếng Việt; trường hợp khác dùng tiếng Anh. Lựa chọn được lưu cho tài khoản chạy ứng dụng.

Cửa sổ mạng và tối ưu đang mở cập nhật ngay, giữ nguyên DNS đã nhập, mục đã chọn và thời gian hẹn giờ. Hoàn tất hoặc hủy tác vụ/hộp xác nhận đang mở trước khi đổi.

Để chọn ngôn ngữ cho một lần chạy, mở lệnh tại thư mục chứa EXE:

```powershell
.\win_aio_optimize.exe --lang=en
.\win_aio_optimize.exe --lang=vi
```

Các tham số này không ghi đè ngôn ngữ đã lưu. Cả hai bản dịch được nhúng trong EXE. Tên sản phẩm, định danh gói, kết quả công cụ hệ thống và nội dung hộp thoại Windows giữ nguyên văn bản Windows cung cấp; nhật ký cũ giữ nguyên ngôn ngữ lúc ghi.

</details>

### Tính năng

Khả năng hỗ trợ phụ thuộc phiên bản Windows, build, thành phần đã cài và phần cứng. Ứng dụng báo rõ mục không hỗ trợ. Xem [ghi chú tối ưu](docs/optimizer-notes.md) để biết chính sách sử dụng và chi tiết tương thích.

#### Bảng điều khiển và thông tin máy

- Giao diện mới với thanh công cụ bên trái, tổng quan phần cứng, lối mở khôi phục, hẹn giờ và nhật ký.
- Chế độ Sáng/Tối mặc định theo Windows; có thể chọn riêng cho app và hỗ trợ tương phản cao.
- Thông tin phần cứng theo nhóm: CPU, từng thanh RAM, GPU/VRAM, driver, ổ đĩa và dung lượng trống, mainboard, BIOS, Windows, mạng, pin, chế độ firmware và Secure Boot.
- Làm mới ở nền, lọc thành phần và sao chép từng giá trị. Dữ liệu không có và lỗi truy vấn được ghi rõ.

#### Bảo vệ trước khi thay đổi

- **Tạo bản sao Registry (Khuyên dùng)** — lưu SYSTEM, SOFTWARE và HKCU của tài khoản hiện tại vào thư mục sao lưu riêng tư.
- **Tạo điểm khôi phục hệ thống (Khuyên dùng)** — yêu cầu điểm khôi phục Windows trước các thay đổi cấu hình đã chọn.
- Hai lựa chọn bảo vệ có nhãn rõ ràng ở đầu **Windows Setup**, được đánh dấu sẵn. Chỉ chạy khi bạn xác nhận áp dụng hoặc khôi phục cài đặt Windows tại đây. Nếu bước bảo vệ đã chọn thất bại, thay đổi sẽ dừng lại.
- Windows Setup chia thành các thẻ Quyền riêng tư, AI, Start & Search, Hệ thống, Taskbar và File Explorer; có tìm kiếm và bố cục thích ứng kích thước cửa sổ. Các mục tối ưu mặc định bỏ chọn.
- DNS, WiFi và trang gỡ ứng dụng riêng vẫn có xác nhận, không kèm hai lựa chọn bảo vệ này. Mở thư mục sao lưu hoặc System Protection từ thanh bên.

System Protection phải được bật trước. Windows có thể dùng lại điểm khôi phục gần đây theo chính sách tần suất. Xem [bảo vệ, giao diện và thông tin máy](docs/recovery-and-system-info.md) để biết phạm vi sao lưu và giới hạn.

#### Gỡ ứng dụng tùy chọn

- Xem ứng dụng đã cài thuộc danh sách gói tùy chọn được xác định sẵn.
- Chọn chính xác ứng dụng cần gỡ cho tài khoản hiện tại qua Windows PackageManager.
- Xem danh sách gói và thông báo về dữ liệu có thể bị xóa trước khi đồng ý.

#### Quyền riêng tư và quảng cáo

- Giảm telemetry và thu thập dữ liệu chẩn đoán trong giới hạn của phiên bản Windows.
- Tắt lịch sử hoạt động, theo dõi mở ứng dụng và quảng cáo cá nhân hóa.
- Tắt mẹo, gợi ý và nội dung quảng bá của Windows.
- Giảm quảng cáo, gợi ý và nguồn tin MSN trong Microsoft Edge.

#### Tìm kiếm và AI

- Tắt tích hợp web Bing/Bing AI trong Windows Search và tắt/gỡ Cortana khi hỗ trợ.
- Chặn gợi ý ứng dụng Microsoft Store trong Search trên cấu hình Windows 11 được nhận diện.
- Tắt Copilot và gỡ các gói đã cài của nó cho tài khoản hiện tại.
- Tắt Recall và Click to Do trên các build Windows 11 được hỗ trợ.
- Chuyển khởi động WSAIFabricSvc sang Manual trên Windows 11 được hỗ trợ; Windows vẫn có thể khởi động dịch vụ khi cần.

#### Desktop và File Explorer

- Ẩn widgets/tin tức trên thanh tác vụ và tắt widgets màn hình khóa khi hỗ trợ.
- Ẩn biểu tượng Meet Now trên thanh tác vụ Windows 10.
- Tắt Drag Tray của Windows 11 khi có thiết lập tương ứng.
- Hiển thị phần mở rộng như `.exe`, `.txt` và `.pdf` trong File Explorer.
- Ẩn mục 3D Objects trong This PC trên Windows 10 mà không xóa file bên trong.
- Tắt mạng khi Modern Standby trên phần cứng và cấu hình nguồn Windows 11 được hỗ trợ.

#### Mạng và DNS

- Xem card mạng, địa chỉ IP, MAC và máy chủ DNS.
- Chọn Cloudflare, Google, Quad9, DNS tùy chỉnh hoặc tự động qua DHCP.
- Cấu hình IPv4 và IPv6 riêng biệt; xem card mạng và địa chỉ máy chủ trước khi xác nhận.
- Xóa bộ nhớ đệm DNS hoặc đặt lại IPv4, IPv6 và Winsock, có kết quả từng bước.

Thay đổi DNS có thể ảnh hưởng VPN và mạng doanh nghiệp. Ứng dụng báo rõ khi chỉ thành công một phần; đặt lại mạng yêu cầu khởi động lại máy.

#### Sao lưu và hẹn giờ tắt máy

- Xuất hồ sơ WiFi qua Windows WLAN API, có lựa chọn riêng về xuất mật khẩu.
- Khôi phục hồ sơ XML WiFi đã chọn và báo số lần nhập thành công/thất bại.
- Xuất driver bên thứ ba vào thư mục riêng bằng DISM. Driver tích hợp trong Windows nằm ngoài phạm vi xuất.
- Hẹn tắt máy theo phút hoặc giờ, xem đếm ngược từ lịch đã lưu và hủy yêu cầu.
- Chạy tác vụ lâu ở nền và ghi kết quả, không ghi mật khẩu WiFi vào nhật ký.

Giữ riêng tư bản sao XML chứa mật khẩu WiFi dạng rõ. Xuất driver có thể mất vài phút.

### Khôi phục và tương thích

> [!NOTE]
> Khôi phục cài đặt áp dụng cho lần tối ưu được lưu gần nhất. Chức năng này không cài lại ứng dụng đã gỡ, lấy lại dữ liệu ứng dụng hoặc ảnh chụp Recall đã xóa.

<details>
<summary>Thông tin về bản sao cài đặt, giới hạn Windows và phạm vi tài khoản.</summary>

**Bản sao cài đặt.** Trước khi ghi các thay đổi registry, dịch vụ, nguồn điện hoặc quyền truy cập được hỗ trợ, ứng dụng lưu trạng thái trước đó. Bản sao mới thay thế bản sao cũ. Hủy là lựa chọn mặc định trong hộp xác nhận.

**Hỗ trợ Windows.** Một số chính sách yêu cầu Enterprise/Education hoặc build mới. Home/Pro có thể vẫn gửi dữ liệu chẩn đoán bắt buộc, nên không thể bảo đảm tắt toàn bộ telemetry. Chính sách tổ chức và Windows Update có thể ghi đè thiết lập. Tắt một tính năng AI không xóa mọi thành phần AI. Đăng xuất hoặc khởi động lại khi cần cập nhật giao diện. UI và nhật ký phân biệt thành công, lỗi và thao tác không hỗ trợ.

**Gợi ý Store.** Mục này chặn đọc/ghi dữ liệu cache `store.db` được nhận diện và lưu quyền truy cập ban đầu. Nó có thể ảnh hưởng gợi ý bên trong Microsoft Store. Đây là cách xử lý tương thích, không phải chính sách chính thức của Microsoft.

**Phạm vi tài khoản.** Thiết lập HKCU, thao tác WiFi và gỡ ứng dụng dùng tài khoản hiển thị ở màn hình chính. Nếu UAC dùng thông tin đăng nhập của quản trị viên khác, thay đổi theo người dùng áp dụng cho tài khoản quản trị viên đó.

**Dữ liệu ứng dụng.** Chạy portable vẫn lưu nhật ký và bản sao cài đặt trong `%LOCALAPPDATA%\WinAIOOptimize`. Nhật ký là `activity.log`; ngôn ngữ được lưu tại `HKCU\Software\Doctoten\WinAIOOptimize`.

</details>

### Build và kiểm tra

<details>
<summary>Biên dịch EXE native và chạy kiểm tra chỉ đọc.</summary>

Dùng bộ công cụ x64: **LLVM-MinGW / MinGW-w64** với `gcc`, `windres` trên PATH hoặc **Visual Studio 2022 Build Tools** cùng Windows SDK và CMake.

Từ thư mục gốc repository:

```bat
build.bat
run.bat
```

Kết quả: `dist\win_aio_optimize.exe` và checksum `.sha256`. `run.bat` build ứng dụng nếu chưa có EXE.

Với CMake và Visual Studio:

```powershell
cmake -S . -B build-native -A x64
cmake --build build-native --config Release
ctest --test-dir build-native -C Release --output-on-failure
```

Kết quả: `build-native/bin/Release/win_aio_optimize.exe` và checksum `.sha256`. Với Ninja/MinGW, bỏ `-A x64` và chọn generator/toolchain phù hợp; kết quả nằm trong `build-native/bin/`.

Chạy toàn bộ kiểm tra chỉ đọc, không yêu cầu quyền quản trị hay thay đổi hệ thống:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\validate.ps1
# Tùy chọn: đọc thêm thông tin phần cứng của máy.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\validate.ps1 -Inventory
```

CI coi cảnh báo biên dịch là lỗi và chạy cùng script kiểm tra với `-Binary build-native/bin/Release/win_aio_optimize.exe`. Script kiểm tra UTF-8, cú pháp PowerShell, phiên bản đồng bộ, thông tin EXE và SHA256, sau đó chạy self-test cùng cả bốn tổ hợp Anh/Việt và Sáng/Tối. Kiểm tra bao gồm DNS, giới hạn hẹn giờ, ranh giới tối ưu/gỡ gói, xử lý lỗi bảo vệ, định dạng thông tin máy và đổi ngôn ngữ không mất dữ liệu nhập. Nhật ký được lưu trong `build-native/validation-*` và tải lên CI. Dùng `-SourceOnly` để kiểm tra source khi chưa build EXE.

Thay đổi hệ thống thực tế cần kiểm thử trên VM Windows 10/11 dùng riêng, có snapshot. Xem [TESTING.md](TESTING.md) để biết kết quả và danh sách nghiệm thu. Không áp dụng tối ưu, đổi DNS, gỡ ứng dụng hoặc hẹn tắt máy chỉ để kiểm thử trên máy phát triển.

Source native và tài nguyên nằm trong `src/`; tài liệu chính sách ở `docs/`. Các thư mục sinh tự động `build-native/` và `dist/` được loại khỏi Git. Liên kết GitHub giữ tên repository `Doctoten/Windows-utility-tool`.

</details>

### Góp ý và đóng góp

Báo lỗi hoặc đề xuất tính năng qua [GitHub Issues](https://github.com/Doctoten/Windows-utility-tool/issues). Ghi rõ phiên bản ứng dụng, phiên bản/build Windows, các bước tái hiện và mã lỗi liên quan. Xóa thông tin cá nhân trong nhật ký; không đính kèm mật khẩu WiFi hoặc file sao lưu riêng tư.

Hoan nghênh đóng góp mã nguồn và bản dịch. Với mã thay đổi hệ thống, mô tả phiên bản Windows bị ảnh hưởng, nội dung xác nhận cho người dùng và các bước kiểm tra đã thực hiện. Xem [TESTING.md](TESTING.md).

### Giấy phép

Dự án được phát hành theo [MIT License](LICENSE).

Phát triển bởi **Doctoten**. [Về cách sử dụng](#usage-vi) · [English](#english)
