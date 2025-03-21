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
<model>SMTC-8</model> \
<hardwareVersion>P" CONFIG_BOARD_REVISION "</hardwareVersion> \
<softwareVersion>1.0.0</softwareVersion> \
</identification> \
<acdi /> \
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
<description>Custom Event ID to use to set the switch to reversed. \
Do not use if you are using standard EventIDs for switch \
control</description> \
</eventid> \
<eventid> \
<name>Normal EventID</name> \
<description>Custom Event ID to use to set the switch to normal. Do \
not use if you are using standard EventIDs for switch \
control</description> \
</eventid> \
<int size='2'> \
<name>DCC Switch number</name> \
<description>The DCC switch number to react to. When using standard \
EventIDs, set this value.</description> \
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
<relation> \
<property>4</property> \
<value>Disable Output</value> \
</relation> \
</map> \
</int> \
<group offset='12' /> \
</group> \
</segment> \
<segment space='250'> \
<name>Global Config</name> \
<int size='1'> \
<name>DCC to LCC switch message</name> \
<description>Should this node translate DCC switch messages to well-known LCC messages?(requires reboot to take effect)</description> \
<map> \
<relation> \
<property>0</property> \
<value>Disabled</value> \
</relation> \
<relation> \
<property>1</property> \
<value>Enabled</value> \
</relation> \
<relation> \
<property>2</property> \
<value>Enabled(Save last known state upon shutdown)</value> \
</relation> \
</map> \
</int> \
</segment> \
<segment space='249'> \
<name>Firmware Versions</name> \
<group> \
<name>Primary version</name> \
<int size='1'> \
<name>Major</name> \
</int> \
<int size='1'> \
<name>Minor</name> \
</int> \
<int size='2'> \
<name>Revision</name> \
</int> \
<int size='4'> \
<name>Build Number</name> \
</int> \
</group> \
<group> \
<name>Secondary version</name> \
<int size='1'> \
<name>Major</name> \
</int> \
<int size='1'> \
<name>Minor</name> \
</int> \
<int size='2'> \
<name>Revision</name> \
</int> \
<int size='4'> \
<name>Build Number</name> \
</int> \
</group> \
</segment> \
</cdi>"
};

#endif /* LCC_TORTOISE_ZEPHYR_SRC_TORTOISE_CDI_H_ */
