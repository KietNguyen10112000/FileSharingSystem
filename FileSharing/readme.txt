Mã nguồn cần được build cùng OpenSSL
https://github.com/openssl/openssl

Mã nguồn build tốt trên Windows với MSBuild.

Build lần lượt

1. FileSystemProtection
2. FileSharingClient
3. FileSharingHost

Copy tất cả các file FileSystemProtection.exe, FileSharingClient.exe, FileSharingHost.exe, test.key, test.der (ở X509) vào cùng một folder.

Chạy file FileSharingHost.exe.
Khi chạy có thể thiếu một số DLL file, chỉ cần tải đầy đủ là có thể chạy được.
