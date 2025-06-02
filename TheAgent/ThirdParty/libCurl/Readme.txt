1. get curl in GitHub and put to local folder C:\codes\TheAgent\ThirdParty\curl-8.10.1
2. Launch command prompt of "X64 Native Tools Command Prompt"
3. invoke "cd C:\codes\TheAgent\ThirdParty\curl-8.10.1\winbuild"
4. invoke "nmake /f Makefile.vc mode=static RTLIBCFG=static"
5. Get the output folders in "C:\codes\TheAgent\ThirdParty\curl-8.10.1\builds"