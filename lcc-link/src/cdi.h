#ifndef CDI_H
#define CDI_H

const char cdi[] = {
"<?xml version='1.0'?> \
<cdi xmlns:xsi='http://www.w3.org/2001/XMLSchema-instance' xsi:noNamespaceSchemaLocation='http://openlcb.org/schema/cdi/1/1/cdi.xsd'> \
<identification> \
<manufacturer>Snowball Creek Electronics</manufacturer> \
<model>LCC-Link</model> \
<hardwareVersion>R1</hardwareVersion> \
<softwareVersion>" CONFIG_MCUBOOT_IMGTOOL_SIGN_VERSION "</softwareVersion> \
</identification> \
<acdi/> \
<segment space='251'> \
<name>Node ID</name> \
<group> \
<name>Your name and description for this node</name> \
<string size='63'> \
<name>Node Name</name> \
</string> \
<string size='64' offset='1'> \
<name>Node Description</name> \
</string> \
</group> \
</segment> \
<segment space='253'> \
<name>DCC Decoding</name> \
<int size='1'> \
<name>Pass Idle Packets</name> \
<description>Should DCC idle packets be forwarded to the computer?</description> \
<map> \
<relation> \
<property>0</property> \
<value>Disabled</value> \
</relation> \
<relation> \
<property>1</property> \
<value>Enabled</value> \
</relation> \
</map> \
</int> \
</segment> \
</cdi>"
};

#endif
