uint32_t chipId = 0;

void setup() {
	Serial.begin(115200);

	uint64_t chipid;
	chipid=ESP.getEfuseMac();//The chip ID is essentially its MAC address(length: 6 bytes).

	Serial.printf("ESP32 MAC = %08X\r\n",chipid);
	Serial.printf("ESP32 Chip ID = %04X\r\n",(uint16_t)(chipid>>32));//print High 2 bytes
 
	Serial.printf("Chip Revision %d\r\n", ESP.getChipRevision());
	esp_chip_info_t chip_info;
	esp_chip_info(&chip_info);
	Serial.printf("Number of Core: %d\r\n", chip_info.cores);
	Serial.printf("CPU Frequency: %d MHz\r\n", ESP.getCpuFreqMHz());	
	Serial.printf("Flash Chip Size = %d byte\r\n", ESP.getFlashChipSize());
	Serial.printf("Flash Frequency = %d Hz\r\n", ESP.getFlashChipSpeed());
	Serial.printf("ESP-IDF version = %s\r\n", esp_get_idf_version());
	//利用可能なヒープのサイズを取得
	Serial.printf("Available Heap Size= %d\r\n", esp_get_free_heap_size());
	//利用可能な内部ヒープのサイズを取得
	Serial.printf("Available Internal Heap Size = %d\r\n", esp_get_free_internal_heap_size());
	//これまでに利用可能だった最小ヒープを取得します
	Serial.printf("Minimum Free Heap Ever Available Size = %d\r\n", esp_get_minimum_free_heap_size());
	Serial.println();
 
	uint8_t mac0[6];
	esp_efuse_mac_get_default(mac0);
	Serial.printf("Default Mac Address = %02X:%02X:%02X:%02X:%02X:%02X\r\n", mac0[0], mac0[1], mac0[2], mac0[3], mac0[4], mac0[5]);
  char mymac[13];
  sprintf(mymac,"%02X%02X%02X%02X%02X%02X", mac0[0], mac0[1], mac0[2], mac0[3], mac0[4], mac0[5]);  
  Serial.printf("MAC Adress: %s\r\n",mymac);
	uint8_t mac3[6];
	esp_read_mac(mac3, ESP_MAC_WIFI_STA);
	Serial.printf("[Wi-Fi Station] Mac Address = %02X:%02X:%02X:%02X:%02X:%02X\r\n", mac3[0], mac3[1], mac3[2], mac3[3], mac3[4], mac3[5]);
 
	uint8_t mac4[7];
	esp_read_mac(mac4, ESP_MAC_WIFI_SOFTAP);
	Serial.printf("[Wi-Fi SoftAP] Mac Address = %02X:%02X:%02X:%02X:%02X:%02X\r\n", mac4[0], mac4[1], mac4[2], mac4[3], mac4[4], mac4[5]);
 
	uint8_t mac5[6];
	esp_read_mac(mac5, ESP_MAC_BT);
	Serial.printf("[Bluetooth] Mac Address = %02X:%02X:%02X:%02X:%02X:%02X\r\n", mac5[0], mac5[1], mac5[2], mac5[3], mac5[4], mac5[5]);
 
	uint8_t mac6[6];
	esp_read_mac(mac6, ESP_MAC_ETH);
	Serial.printf("[Ethernet] Mac Address = %02X:%02X:%02X:%02X:%02X:%02X\r\n", mac6[0], mac6[1], mac6[2], mac6[3], mac6[4], mac6[5]);
}

void loop() {

}
