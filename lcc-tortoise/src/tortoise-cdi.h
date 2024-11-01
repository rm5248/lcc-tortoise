/*
 * tortoise-cdi.h
 *
 *  Created on: Jun 17, 2024
 *      Author: robert
 */

#ifndef LCC_TORTOISE_ZEPHYR_SRC_TORTOISE_CDI_H_
#define LCC_TORTOISE_ZEPHYR_SRC_TORTOISE_CDI_H_

const char cdi[] = {
"<?xml version='1.0'?> \
<cdi xmlns:xsi='http://www.w3.org/2001/XMLSchema-instance' xsi:noNamespaceSchemaLocation='http://openlcb.org/schema/cdi/1/1/cdi.xsd'> \
<identification> \
<manufacturer>Snowball Creek</manufacturer> \
<model>Tortoise LCC-8</model> \
<hardwareVersion>0.1</hardwareVersion> \
<softwareVersion>0.1</softwareVersion> \
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
<name>Outputs</name> \
<group replication='8'> \
<name>OUT</name> \
<repname>Output</repname> \
<eventid> \
<name>Reversed EventID</name> \
<description>Custom Event ID to use to set the switch to reversed.  Do not use if you are using standard EventIDs for switch control</description> \
</eventid> \
<eventid> \
<name>Normal EventID</name> \
<description>Custom Event ID to use to set the switch to normal.  Do not use if you are using standard EventIDs for switch control</description> \
</eventid> \
<int size='2'> \
<name>DCC Switch number</name> \
<description>The DCC switch number to react to.  When using standard EventIDs, set this value.</description> \
<min>1</min> \
<max>2048</max> \
</int> \
<int size='1'> \
<name>Startup Control</name> \
<description>How this switch should behave on startup</description> \
<map> \
<relation> \
<property>0</property> \
<value>Normal</value> \
</relation> \
<relation> \
<property>1</property> \
<value>Reversed</value> \
</relation> \
<relation> \
<property>2</property> \
<value>Last State</value> \
</relation> \
</map> \
</int> \
<int size='1'> \
<name>Control Type</name> \
<description>How this output is controlled</description> \
<map> \
<relation> \
<property>0</property> \
<value>LCC Only(Standard event IDs)</value> \
</relation> \
<relation> \
<property>1</property> \
<value>DCC Only</value> \
</relation> \
<relation> \
<property>2</property> \
<value>LCC and DCC</value> \
</relation> \
<relation> \
<property>3</property> \
<value>LCC(Custom event IDs)</value> \
</relation> \
</map> \
</int> \
<!-- invisible last known pos, 1 byte --> \
<int size='1' offset='1'> \
<name>Output Type</name> \
<description>How this output is changed</description> \
<map> \
<relation> \
<property>0</property> \
<value>Stall Motor(always-on)</value> \
</relation> \
<relation> \
<property>1</property> \
<value>Pulse</value> \
</relation> \
</map> \
</int> \
<int size='1'> \
<name>Pulse Length</name> \
<description>When output is set to pulse, how long the output is pulsed for</description> \
<map> \
<relation> \
<property>0</property> \
<value>200ms</value> \
</relation> \
<relation> \
<property>1</property> \
<value>400ms</value> \
</relation> \
<relation> \
<property>2</property> \
<value>800ms</value> \
</relation> \
<relation> \
<property>3</property> \
<value>1600ms</value> \
</relation> \
</map> \
</int> \
<!-- align to 32 bytes --> \
<group offset='9'/> \
</group> \
</segment> \
</cdi>"
};

#endif /* LCC_TORTOISE_ZEPHYR_SRC_TORTOISE_CDI_H_ */
