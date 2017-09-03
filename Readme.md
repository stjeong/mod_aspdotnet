What is it?
================================

This repo is migrated from [mod_aspdotnet](https://sourceforge.net/projects/mod-aspdotnet/) to support Apache 2.4 for my own purpose.

I'm not good at architecture on Apache and its module. My work is only to support for Apache 2.4 and .NET 4.0 or later.

You have to install appropriate modules for your environment as follows.

| Apache Version | mod_aspdotet                          | .NET  |
|----------------|---------------------------------------|-------|
| 2.0            | [mod_aspdotnet-2.0.0.msi](https://archive.apache.org/dist/httpd/mod_aspdotnet/mod_aspdotnet-2.0.0.msi)               | CLR 2 |
| 2.2            | [mod_aspdotnet-2.2.0.2006-setup-r2.msi](http://sourceforge.net/project/platformdownload.php?group_id=175077) | CLR 2 |
| 2.4            | Release at this repo. (Or build for yourself)                         | CLR 4 |

Which means this version only support .NET 4.0 (or later) within Apache 2.4


How to install
================================

Release at this repo only contains two files
* Apache.Web.dll
* mod_aspdotnet.so

You have to install Apache.Web.dll into .NET 4.0 GAC(Global Assembly Cache) using gacutil.exe which is in Windows SDK or your Visual Studio installation folder.

For mod_aspdotnet.so, just copy to Apache modules folder.

At last step, you have to append configuration to your Apache httpd.conf. (Note that it is changed from mod_aspdotnet 2.2)

```
<IfModule mod_aspdotnet.cpp>
    AspNetMount /aspdocs "c:/temp/xampp/aspdocs"
    Alias /aspdocs "c:/temp/xampp/aspdocs"

    <Directory "c:/temp/xampp/aspdocs">
Require all granted
    </Directory>

    AliasMatch /aspnet_client/system_web/(\d+)_(\d+)_(\d+)_(\d+)/(.*) "C:/Windows/Microsoft.NET/Framework/v$1.$2.$3/ASP.NETClientFiles/$4"

    <Directory "C:/Windows/Microsoft.NET/Framework/v*/ASP.NETClientFiles">
    Options FollowSymlinks
    Order allow,deny
    Allow from all
    </Directory>

</IfModule>
```

> Apache.Web.dll and mod_asdpdotnet.so have dependencies on [Microsoft Visual C++ Redistributable for Visual Studio 2017](https://www.visualstudio.com/downloads/)


How to build
================================
You can build this solution in Visual Studio 2017. There is no prerequisites.

* If you need to get binaries for Windows XP/2003, you have to install Visual Studio 2013 for Platform Toolset = 'v120_xp'.

Change Log
================================

2.4 - Sep 3, 2017

* Initial checked-in


Reqeuests or Contributing to Repository
================================
As I said, I just migrated this repo for my private purpose. If you need some features or whatever, make an issue at [https://github.com/stjeong/mod_aspdotnet/issues](https://github.com/stjeong/mod_aspdotnet/issues)

Any help and advices for this repo are welcome. (I just know a few knowledge about .NET Framework and am a totally novice at Apache Server)

License
================================
Apache License V2.0

(Refer to LICENSE.txt)