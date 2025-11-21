/*
 * cdi.h
 *
 *  Created on: Oct 23, 2025
 *      Author: robert
 */

#ifndef LCC_SERVO16_PLUS_SRC_CDI_H_
#define LCC_SERVO16_PLUS_SRC_CDI_H_

const char* cdi = "<?xml version='1.0'?> \
<cdi xmlns:xsi='http://www.w3.org/2001/XMLSchema-instance' xsi:noNamespaceSchemaLocation='http://openlcb.org/schema/cdi/1/1/cdi.xsd'> \
<identification> \
<manufacturer>Snowball Creek Electronics</manufacturer> \
<model>Servo 16 Plus</model> \
<hardwareVersion>0</hardwareVersion> \
<softwareVersion>0</softwareVersion> \
</identification> \
<acdi/> \
<!-- \
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
		</group>	 \
	</segment> \
--> \
<segment space='253'> \
<name>Outputs</name> \
<group replication='4'> \
<repname>PCA9685 Board </repname> \
<int size='1'> \
<name>Board address</name> \
</int> \
<int size='1'> \
<name>Board Type</name> \
<description>Select what this board is controlling(servos or LEDs).  Requires reboot to take effect</description> \
<map> \
<relation> \
<property>0</property> \
<value>Servo Control</value> \
</relation> \
<relation> \
<property>1</property> \
<value>LED Control</value> \
</relation> \
</map> \
</int> \
<!-- six bytes of buffer --> \
<group offset='6'/> \
<group replication='16'> \
<name>OUT</name> \
<repname>Output </repname> \
<string size='32'> \
<name>Output Name</name> \
<description>What this output is for(optional)</description> \
</string> \
<group> \
<name>Servo Settings</name> \
<int size='2'> \
<name>Servo min pulse time(us)</name> \
<description>The minimum servo pulse time.  By convention, this is 1ms(1000us).  Other servos may use a different value.</description> \
<min>700</min> \
<max>1500</max> \
</int> \
<int size='2'> \
<name>Servo max pulse time(us)</name> \
<description>The maximum servo pulse time.  By convention, this is 2ms(2000us).  Other servos may use a different value.</description> \
<min>1500</min> \
<max>2500</max> \
</int> \
<int size='1'> \
<name>Servo Speed</name> \
</int> \
</group> \
<int size='1'> \
<name>Default Setting</name> \
<description>Select which of the 6 event inputs will be the 'default' state of this output</description> \
<map> \
<relation> \
<property>0</property> \
<value>Event Input 1</value> \
</relation> \
<relation> \
<property>1</property> \
<value>Event Input 2</value> \
</relation> \
<relation> \
<property>2</property> \
<value>Event Input 3</value> \
</relation> \
<relation> \
<property>3</property> \
<value>Event Input 4</value> \
</relation> \
<relation> \
<property>4</property> \
<value>Event Input 5</value> \
</relation> \
<relation> \
<property>5</property> \
<value>Event Input 6</value> \
</relation> \
</map> \
</int> \
<!-- 10 reserved bytes --> \
<group offset='10'/> \
<group replication='6'> \
<name>Event Inputs(Consumers)</name> \
<repname>Input </repname> \
<eventid> \
<name>EventID</name> \
<description>Event ID that will be consumed to set output to the given value</description> \
</eventid> \
<int size='2'> \
<name>Argument 1</name> \
<description>When board is used as servo, sets percentage of movement that servo is set to.  When set to LED, first argument(see manual)</description> \
</int> \
<int size='2'> \
<name>Argument 2</name> \
<description>When board is used as servo this argument is ignored.  When set to LED, second argument(see manual)</description> \
</int> \
<int size='1'> \
<name>LED state</name> \
<description>Only relevant if board drives LEDs.  When this event is consumed, the LED will go to this given state</description> \
<map> \
<relation> \
<property>0</property> \
<value>Off</value> \
</relation> \
<relation> \
<property>1</property> \
<value>Steady on</value> \
</relation> \
<relation> \
<property>2</property> \
<value>Pulse Slow</value> \
</relation> \
<relation> \
<property>3</property> \
<value>Pulse Medium</value> \
</relation> \
<relation> \
<property>4</property> \
<value>Pulse Fast</value> \
</relation> \
</map> \
</int> \
<group offset='3'/> \
</group> \
</group> \
</group> \
</segment> \
<!-- \
	<segment space='250'> \
		<name>Global Config</name> \
	</segment> \
 \
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
 \
	<segment space='248'> \
		<name>Voltages</name> \
		<int size='4'> \
			<name>Output Voltage(mv)</name> \
		</int> \
		<int size='4'> \
			<name>Input Voltage(mv)</name> \
		</int> \
	</segment> \
--> \
</cdi>";

#endif /* LCC_SERVO16_PLUS_SRC_CDI_H_ */
