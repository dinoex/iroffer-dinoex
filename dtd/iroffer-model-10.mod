<!--
Iroffer Packlist Markup Language version 1.0

Namespace = http://iroffer.dinoex.net/apps/iroffer-1

This DTD module is identified by the PUBLIC and SYSTEM identifiers:

PUBLIC "-//iroffer.dinoex.net//ELEMENTS iroffer 1.0 Document Model//EN"
SYSTEM "http://iroffer.dinoex.net/dtd/iroffer-model-10.mod"

Copyright (c) 2010 Dirk Meyer
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
SUCH DAMAGE.
-->

<!--
-->
<!ENTITY % iroffer.version.attrib "version (1.0) #IMPLIED" >

<!-- namespace support -->
<!ENTITY % iroffer.xmlns.attrib  "%NS.decl.attrib;" >
<!ENTITY % iroffer.Common.attrib "%iroffer.xmlns.attrib; id ID #IMPLIED" >

<!-- Qualified names -->
<!ENTITY % iroffer.iroffer.qname "%iroffer.pfx;iroffer" >

<!ENTITY % iroffer.packlist.qname "%iroffer.pfx;packlist" >
<!ENTITY % iroffer.pack.qname "%iroffer.pfx;pack" >
<!ENTITY % iroffer.packnr.qname "%iroffer.pfx;packnr" >
<!ENTITY % iroffer.packname.qname "%iroffer.pfx;packname" >
<!ENTITY % iroffer.packsize.qname "%iroffer.pfx;packsize" >
<!ENTITY % iroffer.packbytes.qname "%iroffer.pfx;packbytes" >
<!ENTITY % iroffer.packgets.qname "%iroffer.pfx;packgets" >
<!ENTITY % iroffer.packcolor.qname "%iroffer.pfx;packcolor" >
<!ENTITY % iroffer.adddate.qname "%iroffer.pfx;adddate" >
<!ENTITY % iroffer.md5sum.qname "%iroffer.pfx;md5sum" >
<!ENTITY % iroffer.crc32.qname "%iroffer.pfx;crc32" >
<!ENTITY % iroffer.grouplist.qname "%iroffer.pfx;grouplist" >

<!ENTITY % iroffer.group.qname "%iroffer.pfx;group" >
<!ENTITY % iroffer.groupname.qname "%iroffer.pfx;groupname" >
<!ENTITY % iroffer.groupdesc.qname "%iroffer.pfx;groupdesc" >

<!ENTITY % iroffer.sysinfo.qname "%iroffer.pfx;sysinfo" >
<!ENTITY % iroffer.slots.qname "%iroffer.pfx;slots" >
<!ENTITY % iroffer.slotsfree.qname "%iroffer.pfx;slotsfree" >
<!ENTITY % iroffer.slotsmax.qname "%iroffer.pfx;slotsmax" >
<!ENTITY % iroffer.mainqueue.qname "%iroffer.pfx;mainqueue" >
<!ENTITY % iroffer.idlequeue.qname "%iroffer.pfx;idlequeue" >
<!ENTITY % iroffer.queueuse.qname "%iroffer.pfx;queueuse" >
<!ENTITY % iroffer.queuemax.qname "%iroffer.pfx;queuemax" >
<!ENTITY % iroffer.bandwidth.qname "%iroffer.pfx;bandwidth" >
<!ENTITY % iroffer.banduse.qname "%iroffer.pfx;banduse" >
<!ENTITY % iroffer.bandmax.qname "%iroffer.pfx;bandmax" >
<!ENTITY % iroffer.quota.qname "%iroffer.pfx;quota" >
<!ENTITY % iroffer.packsum.qname "%iroffer.pfx;packsum" >
<!ENTITY % iroffer.diskspace.qname "%iroffer.pfx;diskspace" >
<!ENTITY % iroffer.transfereddaily.qname "%iroffer.pfx;transfereddaily" >
<!ENTITY % iroffer.transferedweekly.qname "%iroffer.pfx;transferedweekly" >
<!ENTITY % iroffer.transferedmonthly.qname "%iroffer.pfx;transferedmonthly" >
<!ENTITY % iroffer.transferedtotal.qname "%iroffer.pfx;transferedtotal" >
<!ENTITY % iroffer.transferedtotalbytes.qname "%iroffer.pfx;transferedtotalbytes" >
<!ENTITY % iroffer.limits.qname "%iroffer.pfx;limits" >
<!ENTITY % iroffer.minspeed.qname "%iroffer.pfx;minspeed" >
<!ENTITY % iroffer.maxspeed.qname "%iroffer.pfx;maxspeed" >
<!ENTITY % iroffer.network.qname "%iroffer.pfx;network" >
<!ENTITY % iroffer.networkname.qname "%iroffer.pfx;networkname" >
<!ENTITY % iroffer.confignick.qname "%iroffer.pfx;confignick" >
<!ENTITY % iroffer.currentnick.qname "%iroffer.pfx;currentnick" >
<!ENTITY % iroffer.servername.qname "%iroffer.pfx;servername" >
<!ENTITY % iroffer.currentservername.qname "%iroffer.pfx;currentservername" >
<!ENTITY % iroffer.channel.qname "%iroffer.pfx;channel" >
<!ENTITY % iroffer.stats.qname "%iroffer.pfx;stats" >
<!ENTITY % iroffer.version.qname "%iroffer.pfx;version" >
<!ENTITY % iroffer.uptime.qname "%iroffer.pfx;uptime" >
<!ENTITY % iroffer.totaluptime.qname "%iroffer.pfx;totaluptime" >
<!ENTITY % iroffer.lastupdate.qname "%iroffer.pfx;lastupdate" >


<!-- Elements and Structure -->

<!--
-->
<!ELEMENT %iroffer.iroffer.qname;
        ( ( %iroffer.packlist.qname; )?,
          ( %iroffer.grouplist.qname; )?,
            %iroffer.sysinfo.qname;)
>
<!ATTLIST %iroffer.iroffer.qname;
        %iroffer.version.attrib;
        %iroffer.Common.attrib;
>

<!--
-->
<!ELEMENT %iroffer.packlist.qname;
        ( %iroffer.pack.qname; )*
>
<!ATTLIST %iroffer.packlist.qname; %iroffer.Common.attrib; >

<!--
-->
<!ELEMENT %iroffer.pack.qname;
        ( %iroffer.packnr.qname;,
          %iroffer.packname.qname;,
          %iroffer.packsize.qname;,
          %iroffer.packbytes.qname;,
          %iroffer.packgets.qname;,
          ( %iroffer.packcolor.qname; )?,
          ( %iroffer.adddate.qname; )?, 
          ( %iroffer.groupname.qname; )?,
          ( %iroffer.md5sum.qname; )?,
          ( %iroffer.crc32.qname; )? )
>
<!ATTLIST %iroffer.pack.qname; %iroffer.Common.attrib; >

<!--
-->
<!ELEMENT %iroffer.packnr.qname; ( #PCDATA ) >
<!ATTLIST %iroffer.packnr.qname; %iroffer.Common.attrib; >

<!--
-->
<!ELEMENT %iroffer.packname.qname; ( #PCDATA ) >
<!ATTLIST %iroffer.packname.qname; %iroffer.Common.attrib; >

<!--
-->
<!ELEMENT %iroffer.packsize.qname; ( #PCDATA ) >
<!ATTLIST %iroffer.packsize.qname; %iroffer.Common.attrib; >

<!--
-->
<!ELEMENT %iroffer.packbytes.qname; ( #PCDATA ) >
<!ATTLIST %iroffer.packbytes.qname; %iroffer.Common.attrib; >

<!--
-->
<!ELEMENT %iroffer.packgets.qname; ( #PCDATA ) >
<!ATTLIST %iroffer.packgets.qname; %iroffer.Common.attrib; >

<!--
-->
<!ELEMENT %iroffer.packcolor.qname; ( #PCDATA ) >
<!ATTLIST %iroffer.packcolor.qname; %iroffer.Common.attrib; >

<!--
-->
<!ELEMENT %iroffer.adddate.qname; ( #PCDATA ) >
<!ATTLIST %iroffer.adddate.qname; %iroffer.Common.attrib; >

<!--
-->
<!ELEMENT %iroffer.md5sum.qname; ( #PCDATA ) >
<!ATTLIST %iroffer.md5sum.qname; %iroffer.Common.attrib; >

<!--
-->
<!ELEMENT %iroffer.crc32.qname; ( #PCDATA ) >
<!ATTLIST %iroffer.crc32.qname; %iroffer.Common.attrib; >


<!--
-->
<!ELEMENT %iroffer.grouplist.qname;
        ( %iroffer.group.qname; )*
>
<!ATTLIST %iroffer.grouplist.qname; %iroffer.Common.attrib; >

<!--
-->
<!ELEMENT %iroffer.group.qname;
        ( %iroffer.groupname.qname;,
          %iroffer.groupdesc.qname; )
>
<!ATTLIST %iroffer.group.qname; %iroffer.Common.attrib; >

<!--
-->
<!ELEMENT %iroffer.groupname.qname; ( #PCDATA ) >
<!ATTLIST %iroffer.groupname.qname; %iroffer.Common.attrib; >

<!--
-->
<!ELEMENT %iroffer.groupdesc.qname; ( #PCDATA ) >
<!ATTLIST %iroffer.groupdesc.qname; %iroffer.Common.attrib; >


<!--
-->
<!ELEMENT %iroffer.sysinfo.qname;
        (   %iroffer.slots.qname;,
          ( %iroffer.mainqueue.qname; )?,
          ( %iroffer.idlequeue.qname; )?,
          ( %iroffer.bandwidth.qname; )?,
          ( %iroffer.quota.qname; )?,
          ( %iroffer.limits.qname; )?,
          ( %iroffer.network.qname; )*,
          ( %iroffer.stats.qname; )? )
>
<!ATTLIST %iroffer.sysinfo.qname; %iroffer.Common.attrib; >

<!--
-->
<!ELEMENT %iroffer.slots.qname;
        ( %iroffer.slotsfree.qname;,
          %iroffer.slotsmax.qname; )
>
<!ATTLIST %iroffer.slots.qname; %iroffer.Common.attrib; >

<!--
-->
<!ELEMENT %iroffer.slotsfree.qname; ( #PCDATA ) >
<!ATTLIST %iroffer.slotsfree.qname; %iroffer.Common.attrib; >

<!--
-->
<!ELEMENT %iroffer.slotsmax.qname; ( #PCDATA ) >
<!ATTLIST %iroffer.slotsmax.qname; %iroffer.Common.attrib; >

<!--
-->
<!ELEMENT %iroffer.mainqueue.qname;
        ( %iroffer.queueuse.qname;,
          %iroffer.queuemax.qname; )
>
<!ATTLIST %iroffer.mainqueue.qname; %iroffer.Common.attrib; >

<!--
-->
<!ELEMENT %iroffer.idlequeue.qname;
        ( %iroffer.queueuse.qname;,
          %iroffer.queuemax.qname; )
>
<!ATTLIST %iroffer.idlequeue.qname; %iroffer.Common.attrib; >

<!--
-->
<!ELEMENT %iroffer.queueuse.qname; ( #PCDATA ) >
<!ATTLIST %iroffer.queueuse.qname; %iroffer.Common.attrib; >

<!--
-->
<!ELEMENT %iroffer.queuemax.qname; ( #PCDATA ) >
<!ATTLIST %iroffer.queuemax.qname; %iroffer.Common.attrib; >


<!--
-->
<!ELEMENT %iroffer.bandwidth.qname;
        ( %iroffer.banduse.qname;,
          %iroffer.bandmax.qname; )
>
<!ATTLIST %iroffer.bandwidth.qname; %iroffer.Common.attrib; >

<!--
-->
<!ELEMENT %iroffer.banduse.qname; ( #PCDATA ) >
<!ATTLIST %iroffer.banduse.qname; %iroffer.Common.attrib; >

<!--
-->
<!ELEMENT %iroffer.bandmax.qname; ( #PCDATA ) >
<!ATTLIST %iroffer.bandmax.qname; %iroffer.Common.attrib; >

<!--
-->
<!ELEMENT %iroffer.quota.qname;
        ( %iroffer.packsum.qname;,
          %iroffer.diskspace.qname;,
          %iroffer.transfereddaily.qname;,
          %iroffer.transferedweekly.qname;,
          %iroffer.transferedmonthly.qname;,
          %iroffer.transferedtotal.qname;,
          %iroffer.transferedtotalbytes.qname; )
>
<!ATTLIST %iroffer.quota.qname; %iroffer.Common.attrib; >

<!--
-->
<!ELEMENT %iroffer.packsum.qname; ( #PCDATA ) >
<!ATTLIST %iroffer.packsum.qname; %iroffer.Common.attrib; >

<!--
-->
<!ELEMENT %iroffer.diskspace.qname; ( #PCDATA ) >
<!ATTLIST %iroffer.diskspace.qname; %iroffer.Common.attrib; >

<!--
-->
<!ELEMENT %iroffer.transfereddaily.qname; ( #PCDATA ) >
<!ATTLIST %iroffer.transfereddaily.qname; %iroffer.Common.attrib; >

<!--
-->
<!ELEMENT %iroffer.transferedweekly.qname; ( #PCDATA ) >
<!ATTLIST %iroffer.transferedweekly.qname; %iroffer.Common.attrib; >

<!--
-->
<!ELEMENT %iroffer.transferedmonthly.qname; ( #PCDATA ) >
<!ATTLIST %iroffer.transferedmonthly.qname; %iroffer.Common.attrib; >

<!--
-->
<!ELEMENT %iroffer.transferedtotal.qname; ( #PCDATA ) >
<!ATTLIST %iroffer.transferedtotal.qname; %iroffer.Common.attrib; >

<!--
-->
<!ELEMENT %iroffer.transferedtotalbytes.qname; ( #PCDATA ) >
<!ATTLIST %iroffer.transferedtotalbytes.qname; %iroffer.Common.attrib; >

<!--
-->
<!ELEMENT %iroffer.limits.qname;
        ( %iroffer.minspeed.qname;,
          %iroffer.maxspeed.qname; )
>
<!ATTLIST %iroffer.limits.qname; %iroffer.Common.attrib; >

<!--
-->
<!ELEMENT %iroffer.minspeed.qname; ( #PCDATA ) >
<!ATTLIST %iroffer.minspeed.qname; %iroffer.Common.attrib; >

<!--
-->
<!ELEMENT %iroffer.maxspeed.qname; ( #PCDATA ) >
<!ATTLIST %iroffer.maxspeed.qname; %iroffer.Common.attrib; >

<!--
-->
<!ELEMENT %iroffer.network.qname;
        (   %iroffer.networkname.qname;,
            %iroffer.confignick.qname;,
            %iroffer.currentnick.qname;,
            %iroffer.servername.qname;,
          ( %iroffer.currentservername.qname; )?,
          ( %iroffer.channel.qname; )* )
>
<!ATTLIST %iroffer.network.qname; %iroffer.Common.attrib; >

<!--
-->
<!ELEMENT %iroffer.networkname.qname; ( #PCDATA ) >
<!ATTLIST %iroffer.networkname.qname; %iroffer.Common.attrib; >

<!--
-->
<!ELEMENT %iroffer.confignick.qname; ( #PCDATA ) >
<!ATTLIST %iroffer.confignick.qname; %iroffer.Common.attrib; >

<!--
-->
<!ELEMENT %iroffer.currentnick.qname; ( #PCDATA ) >
<!ATTLIST %iroffer.currentnick.qname; %iroffer.Common.attrib; >

<!--
-->
<!ELEMENT %iroffer.servername.qname; ( #PCDATA ) >
<!ATTLIST %iroffer.servername.qname; %iroffer.Common.attrib; >

<!--
-->
<!ELEMENT %iroffer.currentservername.qname; ( #PCDATA ) >
<!ATTLIST %iroffer.currentservername.qname; %iroffer.Common.attrib; >

<!--
-->
<!ELEMENT %iroffer.channel.qname; ( #PCDATA ) >
<!ATTLIST %iroffer.channel.qname; %iroffer.Common.attrib; >

<!--
-->
<!ELEMENT %iroffer.stats.qname;
        ( %iroffer.version.qname;,
          %iroffer.uptime.qname;,
          %iroffer.totaluptime.qname;,
          %iroffer.lastupdate.qname; )
>
<!ATTLIST %iroffer.stats.qname; %iroffer.Common.attrib; >

<!--
-->
<!ELEMENT %iroffer.version.qname; ( #PCDATA ) >
<!ATTLIST %iroffer.version.qname; %iroffer.Common.attrib; >

<!--
-->
<!ELEMENT %iroffer.uptime.qname; ( #PCDATA ) >
<!ATTLIST %iroffer.uptime.qname; %iroffer.Common.attrib; >

<!--
-->
<!ELEMENT %iroffer.totaluptime.qname; ( #PCDATA ) >
<!ATTLIST %iroffer.totaluptime.qname; %iroffer.Common.attrib; >

<!--
-->
<!ELEMENT %iroffer.lastupdate.qname; ( #PCDATA ) >
<!ATTLIST %iroffer.lastupdate.qname; %iroffer.Common.attrib; >


<!ENTITY % xhtml-basic-model.mod
        PUBLIC "-//W3C//ENTITIES XHTML Basic 1.0 Document Model 1.0//EN"
        "http://www.w3.org/TR/xhtml-basic/xhtml-basic10-model-1.mod"
>
%xhtml-basic-model.mod;
