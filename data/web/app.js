/**
 * ESP32-S3 Smart Pump Control - Frontend Application
 */

let currentPumpGear = 0;
let isUpdatingStatus = false;

/**
 * 格式化字节数为可读字符串
 */
function formatBytes(bytes) {
    if (bytes === undefined || bytes === null || bytes === 0) {
        return '--';
    }
    if (bytes >= 1024 * 1024 * 1024) {
        return (bytes / (1024 * 1024 * 1024)).toFixed(2) + ' GB';
    } else if (bytes >= 1024 * 1024) {
        return (bytes / (1024 * 1024)).toFixed(2) + ' MB';
    } else if (bytes >= 1024) {
        return (bytes / 1024).toFixed(2) + ' KB';
    } else {
        return bytes + ' B';
    }
}

/**
 * 格式化运行时间
 */
function formatUptime(seconds) {
    if (seconds === undefined || seconds === null) {
        return '--';
    }
    const days = Math.floor(seconds / 86400);
    const hours = Math.floor((seconds % 86400) / 3600);
    const mins = Math.floor((seconds % 3600) / 60);
    const secs = seconds % 60;

    if (days > 0) {
        return `${days}天 ${hours}小时`;
    } else if (hours > 0) {
        return `${hours}小时 ${mins}分`;
    } else if (mins > 0) {
        return `${mins}分 ${secs}秒`;
    } else {
        return `${secs}秒`;
    }
}

/**
 * 更新硬件资源监控数据
 */
function updateHardwareResources(data) {
    // 从 resources 对象获取硬件资源数据
    const resources = data.resources || {};

    // DRAM内存
    const dram = resources.dram || {};
    const dramTotal = dram.total || 0;
    const dramFree = dram.free || 0;
    const dramUsed = dramTotal > 0 ? dramTotal - dramFree : 0;
    const dramPercent = dramTotal > 0 ? (dramUsed / dramTotal * 100) : 0;

    document.getElementById('dramBar').style.width = dramPercent + '%';
    document.getElementById('dramText').textContent =
        formatBytes(dramUsed) + ' / ' + formatBytes(dramTotal);

    // PSRAM内存
    const psram = resources.psram || {};
    const psramTotal = psram.total || 0;
    const psramFree = psram.free || 0;
    const psramUsed = psramTotal > 0 ? psramTotal - psramFree : 0;
    const psramPercent = psramTotal > 0 ? (psramUsed / psramTotal * 100) : 0;

    const psramBar = document.getElementById('psramBar');
    const psramText = document.getElementById('psramText');

    if (psramTotal > 0) {
        psramBar.style.width = psramPercent + '%';
        psramText.textContent = formatBytes(psramUsed) + ' / ' + formatBytes(psramTotal);
    } else {
        psramBar.style.width = '0%';
        psramText.textContent = '无 PSRAM';
    }

    // Flash存储
    const flash = resources.flash || {};
    const flashTotal = flash.total || 0;
    document.getElementById('flashTotal').textContent = formatBytes(flashTotal);

    // SPIFFS文件系统
    const spiffs = resources.spiffs || {};
    const spiffsTotal = spiffs.total || 0;
    const spiffsFree = spiffs.free || 0;
    const spiffsUsed = spiffsTotal > 0 ? (spiffsTotal - spiffsFree) : 0;
    const spiffsPercent = spiffsTotal > 0 ? (spiffsUsed / spiffsTotal * 100) : 0;

    document.getElementById('spiffsBar').style.width = spiffsPercent + '%';
    document.getElementById('spiffsText').textContent =
        formatBytes(spiffsFree) + ' / ' + formatBytes(spiffsTotal);

    // TF卡存储
    const sdcard = resources.sdcard || {};
    const sdcardMounted = sdcard.mounted === true || sdcard.mounted === 1;
    const sdcardTotal = sdcard.total || 0;
    const sdcardFree = sdcard.free || 0;

    const sdcardBar = document.getElementById('sdcardBar');
    const sdcardTextEl = document.getElementById('sdcardText');

    if (sdcardMounted) {
        const sdcardUsed = sdcardTotal - sdcardFree;
        const sdcardPercent = sdcardTotal > 0 ? (sdcardUsed / sdcardTotal * 100) : 0;
        sdcardBar.style.width = sdcardPercent + '%';
        sdcardTextEl.textContent = formatBytes(sdcardUsed) + ' / ' + formatBytes(sdcardTotal);
    } else {
        sdcardBar.style.width = '0%';
        sdcardTextEl.textContent = '未挂载';
    }

    // CPU频率
    const cpu = resources.cpu || {};
    document.getElementById('cpuFreq').textContent =
        (cpu.freq_mhz || data.cpu_freq_mhz || 0) + ' MHz';

    // 运行时间
    document.getElementById('uptime').textContent = formatUptime(data.uptime_seconds);
}

/**
 * Update network status from server
 */
async function updateNetworkStatus() {
    try {
        const response = await fetch('/api/network');
        const data = await response.json();

        // STA模式状态
        const staConnected = data.sta_connected === 1;
        document.getElementById('staSSID').textContent = staConnected ? data.sta_ssid : '--';
        document.getElementById('staIP').textContent = data.sta_ip || '--';
        document.getElementById('staRSSI').textContent = staConnected ? data.sta_rssi : '--';

        const staStatusEl = document.getElementById('staStatus');
        if (staConnected) {
            staStatusEl.textContent = '已连接';
            staStatusEl.style.color = '#4CAF50';
        } else {
            staStatusEl.textContent = '未连接';
            staStatusEl.style.color = '#f44336';
        }

        // AP模式状态
        document.getElementById('apSSID').textContent = data.ap_ssid || '--';
        document.getElementById('apIP').textContent = data.ap_ip || '--';
        document.getElementById('apActive').textContent = '运行中';

    } catch (error) {
        console.error('Failed to fetch network status:', error);
    }
}

/**
 * Update system status from server
 */
async function updateStatus() {
    try {
        const response = await fetch('/api/system/status');
        const json = await response.json();
        const data = json.data || {};
        const sensors = data.sensors || {};
        const actuators = data.actuators || {};

        // 热敏电阻 - 原始值和温度
        document.getElementById('thermistorRaw').textContent = sensors.thermistor_raw !== undefined ? sensors.thermistor_raw : '--';
        document.getElementById('thermistorTemp').textContent = sensors.thermistor_temp !== undefined ? sensors.thermistor_temp.toFixed(1) : '--' + '°C';

        // 光敏电阻 - 原始值和光照
        document.getElementById('photosensorRaw').textContent = sensors.photosensor_raw !== undefined ? sensors.photosensor_raw : '--';
        document.getElementById('photosensorLux').textContent = sensors.light !== undefined ? sensors.light.toFixed(0) : '--' + ' lux';

        // DHT11 温湿度
        document.getElementById('dht11Temp').textContent = sensors.dht11_temp !== undefined ? sensors.dht11_temp.toFixed(1) : '--' + '°C';
        document.getElementById('dht11Humidity').textContent = sensors.dht11_humidity !== undefined ? sensors.dht11_humidity.toFixed(1) : '--' + '%';

        // 水泵档位状态
        currentPumpGear = actuators.pump_state || 0;
        const isPumpOn = actuators.pump_state === 1;
        document.getElementById('pumpStatus').textContent = isPumpOn ? '开启' : '关闭';
        document.getElementById('pumpSpeed').textContent = '速度: ' + (actuators.pump_speed || 0) + '%';

        // 更新档位按钮样式
        updateGearButtons(currentPumpGear);

        // 水泵速度滑块（只在开启时更新）
        if (!isUpdatingStatus) {
            document.getElementById('speedSlider').value = actuators.pump_speed || 0;
            document.getElementById('speedDisplay').textContent = actuators.pump_speed || 0;
        }

        // 舵机角度
        document.getElementById('servoAngle').textContent = (actuators.servo_angle || 90) + '°';
        document.getElementById('servoSlider').value = actuators.servo_angle || 90;
        document.getElementById('servoDisplay').textContent = actuators.servo_angle || 90;

        // 更新硬件资源监控
        updateHardwareResources(data);

        // 同时更新网络状态
        updateNetworkStatus();

    } catch (error) {
        console.error('Failed to fetch status:', error);
    }
}

/**
 * 更新档位按钮样式
 */
function updateGearButtons(gear) {
    const buttons = document.querySelectorAll('.btn-gear');
    buttons.forEach(btn => {
        const btnGear = parseInt(btn.getAttribute('data-gear'));
        if (btnGear === gear) {
            btn.classList.add('active');
        } else {
            btn.classList.remove('active');
        }
    });

    // 水泵卡片样式
    const pumpCard = document.getElementById('pumpCard');
    if (gear > 0) {
        pumpCard.classList.add('active');
    } else {
        pumpCard.classList.remove('active');
    }
}

/**
 * 设置水泵档位
 */
async function setPumpGear(gear) {
    try {
        isUpdatingStatus = true;
        await fetch('/api/pump?gear=' + gear);
        await updateStatus();
        isUpdatingStatus = false;
    } catch (error) {
        console.error('Failed to set pump gear:', error);
        isUpdatingStatus = false;
    }
}

/**
 * 设置水泵速度（仅在当前档位下调整）
 */
async function setSpeed(speed) {
    document.getElementById('speedDisplay').textContent = speed;
    if (currentPumpGear > 0) {
        try {
            await fetch('/api/pump?action=on&speed=' + speed);
            await updateStatus();
        } catch (error) {
            console.error('Failed to set speed:', error);
        }
    }
}

/**
 * Set servo angle
 */
async function setServo(angle) {
    try {
        await fetch('/api/servo?angle=' + angle);
        updateStatus();
    } catch (error) {
        console.error('Failed to set servo:', error);
    }
}

// Event Listeners - 水泵档位
document.getElementById('btnGearOff').addEventListener('click', () => setPumpGear(0));
document.getElementById('btnGearLow').addEventListener('click', () => setPumpGear(1));
document.getElementById('btnGearMedium').addEventListener('click', () => setPumpGear(2));
document.getElementById('btnGearHigh').addEventListener('click', () => setPumpGear(3));

// Event Listeners - 速度滑块
document.getElementById('speedSlider').addEventListener('input', (e) => {
    document.getElementById('speedDisplay').textContent = e.target.value;
});
document.getElementById('speedSlider').addEventListener('change', (e) => {
    setSpeed(e.target.value);
});

// Event Listeners - 舵机
document.getElementById('servoSlider').addEventListener('input', (e) => {
    document.getElementById('servoDisplay').textContent = e.target.value;
});
document.getElementById('servoSlider').addEventListener('change', (e) => {
    setServo(e.target.value);
});

// Initial load and periodic update
updateStatus();
setInterval(updateStatus, 2000);
