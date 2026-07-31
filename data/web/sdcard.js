/**
 * TF卡文件管理器 JavaScript
 * 版本: 1.2.0
 */

// 版本信息
const VERSION = '1.2.0';
console.log('[SDCard] TF卡文件管理器 v' + VERSION + ' 已加载');

// 当前路径
let currentPath = '';

// 文件类型图标
const icons = {
    folder: '📁',
    image: '🖼️',
    video: '🎬',
    audio: '🎵',
    pdf: '📄',
    text: '📝',
    code: '💻',
    archive: '📦',
    file: '📄'
};

// 支持预览的图片类型
const imageExtensions = ['jpg', 'jpeg', 'png', 'gif', 'bmp', 'webp', 'ico'];

// 支持播放的音频类型
const audioExtensions = ['mp3', 'wav', 'flac', 'aac', 'ogg', 'm4a'];

/**
 * 导航到指定目录
 */
function navigateTo(path) {
    currentPath = path;
    loadFiles();
    // 更新 URL 而不刷新页面
    const newUrl = path ? '?path=' + encodeURIComponent(path) : window.location.pathname;
    history.pushState({ path: path }, '', newUrl);
}

/**
 * 加载存储信息
 */
async function loadInfo() {
    try {
        const response = await fetch('/api/sdcard/info');
        if (!response.ok) {
            throw new Error('API请求失败');
        }
        const data = await response.json();

        const totalEl = document.getElementById('total');
        const usedEl = document.getElementById('used');
        const freeEl = document.getElementById('free');
        const progressEl = document.getElementById('progress');

        if (!totalEl || !usedEl || !freeEl || !progressEl) return;

        // 检查挂载状态 - API返回字符串 "true"/"false"
        const isMounted = data.mounted === true || data.mounted === 'true';

        if (isMounted) {
            totalEl.textContent = formatBytes(data.total);
            usedEl.textContent = formatBytes(data.used);
            freeEl.textContent = formatBytes(data.free);

            const percent = data.total > 0 ? (data.used / data.total) * 100 : 0;
            progressEl.style.width = percent + '%';
            progressEl.parentElement.style.display = 'block';
        } else {
            totalEl.textContent = '未挂载';
            usedEl.textContent = '--';
            freeEl.textContent = '--';
            progressEl.style.width = '0%';
        }
    } catch (e) {
        console.error('加载存储信息失败:', e);
        const totalEl = document.getElementById('total');
        if (totalEl) totalEl.textContent = '加载失败';
    }
}

/**
 * 格式化字节数
 */
function formatBytes(bytes) {
    if (bytes === undefined || bytes === null || bytes === 0) {
        return '0 B';
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
 * 获取文件图标类型
 */
function getIconType(name, isDir) {
    if (isDir) return 'folder';

    const ext = getFileExtension(name).toLowerCase();

    if (imageExtensions.includes(ext)) return 'image';
    if (audioExtensions.includes(ext)) return 'audio';

    const videoExts = ['mp4', 'avi', 'mkv', 'mov', 'webm', 'flv', 'wmv', '3gp', 'mvi'];
    if (videoExts.includes(ext)) return 'video';

    if (ext === 'pdf') return 'pdf';

    const textExts = ['txt', 'log', 'md', 'json', 'xml', 'yaml', 'yml', 'ini', 'cfg', 'conf'];
    if (textExts.includes(ext)) return 'text';

    const codeExts = ['c', 'h', 'cpp', 'py', 'js', 'html', 'css', 'java', 'go', 'rs', 'sh'];
    if (codeExts.includes(ext)) return 'code';

    const archiveExts = ['zip', 'rar', '7z', 'tar', 'gz', 'bz2'];
    if (archiveExts.includes(ext)) return 'archive';

    return 'file';
}

/**
 * 获取文件扩展名
 */
function getFileExtension(filename) {
    const dotIndex = filename.lastIndexOf('.');
    if (dotIndex === -1 || dotIndex === 0) return '';
    return filename.substring(dotIndex + 1);
}

/**
 * 加载文件列表
 */
async function loadFiles() {
    const list = document.getElementById('fileList');
    if (!list) return;

    list.innerHTML = '<div class="loading">加载中...</div>';

    const url = '/api/sdcard/files?path=' + encodeURIComponent(currentPath);

    try {
        const response = await fetch(url);
        if (!response.ok) {
            throw new Error('请求失败: ' + response.status);
        }

        const data = await response.json();

        if (data.error) {
            list.innerHTML = '<div class="empty">' + data.error + '</div>';
            return;
        }

        // 更新路径导航
        updatePathNav();

        // 渲染文件列表
        if (!data.files || data.files.length === 0) {
            list.innerHTML = '<div class="empty">📂 目录为空</div>';
            return;
        }

        // 先排序：文件夹在前，然后按名称排序
        const sortedFiles = [...data.files].sort((a, b) => {
            if (a.is_dir !== b.is_dir) return b.is_dir - a.is_dir;
            return a.name.localeCompare(b.name, undefined, { sensitivity: 'base' });
        });

        console.log('[SDCard] 文件列表:', data.files);

        let html = '';
        for (const f of sortedFiles) {
            const icon = icons[getIconType(f.name, f.is_dir)] || icons.file;
            const fullPath = currentPath ? currentPath + '/' + f.name : f.name;
            const iconType = getIconType(f.name, f.is_dir);
            
            // 直接使用后端返回的 URL
            const fileUrl = f.url || fullPath;

            let itemClass = 'file-item';
            if (f.is_dir) {
                itemClass += ' directory';
            } else if (iconType === 'audio') {
                itemClass += ' audio-file';
            } else if (iconType === 'video') {
                itemClass += ' video-file';
            }
            html += '<div class="' + itemClass + '" ';
            html += 'onclick="' + (f.is_dir
                ? 'navigateTo(\'' + escapeQuote(fullPath) + '\')'
                : 'handleFileClick(\'' + escapeQuote(f.name) + '\',\'' + iconType + '\',\'' + escapeQuote(fileUrl) + '\')') + '">';
            html += '<span class="file-icon">' + icon + '</span>';
            html += '<div class="file-name" title="' + escapeHtml(f.name) + '">' + escapeHtml(f.name) + '</div>';
            html += '<div class="file-meta">';
            html += '<span>' + (f.is_dir ? '--' : f.size_str) + '</span>';
            html += '</div></div>';
        }

        list.innerHTML = html;
    } catch (e) {
        console.error('加载文件列表失败:', e);
        list.innerHTML = '<div class="empty">加载失败: ' + escapeHtml(e.message) + '</div>';
    }
}

/**
 * 处理文件点击
 */
function handleFileClick(name, iconType, path) {
    switch (iconType) {
        case 'image':
            openImagePreview(name, path);
            break;
        case 'audio':
            playAudio(name, path);
            break;
        case 'video':
            playVideo(name, path);
            break;
        default:
            downloadFile(path);
    }
}

/**
 * 打开图片预览
 */
function openImagePreview(name, path) {
    const previewImage = document.getElementById('previewImage');
    const previewName = document.getElementById('previewName');
    const previewModal = document.getElementById('previewModal');

    if (!previewImage || !previewModal) return;

    // 使用查询参数方式传递路径，避免 URI 编码问题
    const src = '/fs/files?path=' + encodeURIComponent(path);

    previewImage.src = src;
    if (previewName) previewName.textContent = name;
    previewModal.classList.add('active');
}

/**
 * 播放音频
 */
function playAudio(name, path) {
    const audioPlayer = document.getElementById('audioPlayer');
    const audioElement = document.getElementById('audioElement');
    const playerName = document.getElementById('playerName');

    if (!audioPlayer || !audioElement) return;

    // 使用查询参数方式传递路径，避免 URI 编码问题
    const src = '/fs/files?path=' + encodeURIComponent(path);

    console.log('[SDCard] playAudio:', { name: name, path: path, src: src });

    audioElement.src = src;
    audioElement.load(); // 重新加载音频
    if (playerName) playerName.textContent = name;
    audioPlayer.classList.add('active');

    // 等待加载完成后播放
    audioElement.oncanplay = function() {
        audioElement.play().catch(e => {
            console.error('播放失败:', e);
        });
    };

    // 错误处理
    audioElement.onerror = function() {
        console.error('[SDCard] 音频加载失败:', src);
        console.error('[SDCard] 音频错误码:', audioElement.error ? audioElement.error.code : 'unknown');
        alert('音频加载失败，请检查文件是否存在\n路径: ' + src);
    };
}

/**
 * 播放视频
 */
function playVideo(name, path) {
    const videoPlayer = document.getElementById('videoPlayer');
    const videoElement = document.getElementById('videoElement');
    const playerName = document.getElementById('videoPlayerName');

    if (!videoPlayer || !videoElement) return;

    // 使用查询参数方式传递路径，避免 URI 编码问题
    const src = '/fs/files?path=' + encodeURIComponent(path);

    console.log('[SDCard] playVideo:', { name: name, path: path, src: src });

    videoElement.src = src;
    videoElement.load(); // 重新加载视频
    if (playerName) playerName.textContent = name;
    videoPlayer.classList.add('active');

    // 等待加载完成后播放
    videoElement.oncanplay = function() {
        videoElement.play().catch(e => {
            console.error('视频播放失败:', e);
        });
    };

    // 错误处理
    videoElement.onerror = function() {
        console.error('[SDCard] 视频加载失败:', src);
        console.error('[SDCard] 视频错误码:', videoElement.error ? videoElement.error.code : 'unknown');
        alert('视频加载失败，请检查文件是否存在\n路径: ' + src);
    };
}

/**
 * 关闭音频播放器
 */
function closePlayer() {
    const audioPlayer = document.getElementById('audioPlayer');
    const audioElement = document.getElementById('audioElement');

    if (audioElement) {
        audioElement.pause();
        audioElement.currentTime = 0;
    }
    if (audioPlayer) {
        audioPlayer.classList.remove('active');
    }
}

/**
 * 关闭视频播放器
 */
function closeVideoPlayer() {
    const videoPlayer = document.getElementById('videoPlayer');
    const videoElement = document.getElementById('videoElement');

    if (videoElement) {
        videoElement.pause();
        videoElement.currentTime = 0;
    }
    if (videoPlayer) {
        videoPlayer.classList.remove('active');
    }
}

/**
 * 下载文件
 */
function downloadFile(path) {
    // 使用查询参数方式传递路径，避免 URI 编码问题
    const url = '/fs/files?path=' + encodeURIComponent(path);
    window.open(url, '_blank');
}

/**
 * 关闭预览
 */
function closePreview() {
    const previewModal = document.getElementById('previewModal');
    if (previewModal) {
        previewModal.classList.remove('active');
    }
}

/**
 * 触发文件上传
 */
function triggerUpload() {
    const fileInput = document.getElementById('fileInput');
    if (fileInput) {
        fileInput.click();
    }
}

/**
 * 处理文件选择
 */
function handleFileSelect(input) {
    const file = input.files[0];
    if (!file) return;

    console.log('[SDCard] 选择上传文件:', file.name, formatBytes(file.size));

    // 检查空间
    checkSpaceAndUpload(file);
}

/**
 * 检查空间并上传文件
 */
async function checkSpaceAndUpload(file) {
    try {
        const response = await fetch('/api/sdcard/info');
        const data = await response.json();

        if (file.size > data.free) {
            alert('磁盘空间不足！\n文件大小: ' + formatBytes(file.size) + '\n可用空间: ' + formatBytes(data.free));
            return;
        }

        // 上传文件
        uploadFile(file);
    } catch (e) {
        console.error('检查空间失败:', e);
        alert('检查空间失败');
    }
}

/**
 * 上传文件
 */
async function uploadFile(file) {
    const formData = new FormData();
    formData.append('file', file);

    // 显示上传进度
    showUploadProgress(file.name, file.size);

    try {
        const xhr = new XMLHttpRequest();

        // 进度回调
        xhr.upload.onprogress = function(e) {
            if (e.lengthComputable) {
                const percent = Math.round((e.loaded / e.total) * 100);
                updateUploadProgress(percent);
            }
        };

        // 完成回调
        xhr.onload = function() {
            if (xhr.status === 200 || xhr.status === 201) {
                hideUploadProgress();
                alert('上传成功！\n' + file.name);
                // 刷新文件列表
                loadFiles();
            } else {
                hideUploadProgress();
                alert('上传失败: ' + xhr.status);
            }
        };

        // 错误回调
        xhr.onerror = function() {
            hideUploadProgress();
            alert('上传失败，请检查网络连接');
        };

        // 发送请求 - 文件名通过 URL 参数传递
        const uploadUrl = '/fs/upload?path=' + encodeURIComponent(currentPath) + '&filename=' + encodeURIComponent(file.name);
        xhr.open('POST', uploadUrl);
        xhr.send(formData);

    } catch (e) {
        hideUploadProgress();
        console.error('上传异常:', e);
        alert('上传异常: ' + e.message);
    }
}

/**
 * 显示上传进度
 */
function showUploadProgress(filename, fileSize) {
    let overlay = document.getElementById('uploadOverlay');
    if (!overlay) {
        overlay = document.createElement('div');
        overlay.id = 'uploadOverlay';
        overlay.className = 'upload-overlay';
        overlay.innerHTML = `
            <div class="upload-dialog">
                <div class="upload-title">上传中...</div>
                <div class="upload-filename" id="uploadFilename"></div>
                <div class="upload-progress-bar">
                    <div class="upload-progress-fill" id="uploadProgressFill"></div>
                </div>
                <div class="upload-percent" id="uploadPercent">0%</div>
            </div>
        `;
        document.body.appendChild(overlay);
    }

    document.getElementById('uploadFilename').textContent = filename + ' (' + formatBytes(fileSize) + ')';
    document.getElementById('uploadProgressFill').style.width = '0%';
    document.getElementById('uploadPercent').textContent = '0%';
    overlay.style.display = 'flex';
}

/**
 * 更新上传进度
 */
function updateUploadProgress(percent) {
    const fill = document.getElementById('uploadProgressFill');
    const percentText = document.getElementById('uploadPercent');
    if (fill) fill.style.width = percent + '%';
    if (percentText) percentText.textContent = percent + '%';
}

/**
 * 隐藏上传进度
 */
function hideUploadProgress() {
    const overlay = document.getElementById('uploadOverlay');
    if (overlay) {
        overlay.style.display = 'none';
    }
    // 清空文件选择
    const fileInput = document.getElementById('fileInput');
    if (fileInput) fileInput.value = '';
}

/**
 * HTML转义
 */
function escapeHtml(str) {
    const div = document.createElement('div');
    div.textContent = str;
    return div.innerHTML;
}

/**
 * 转义单引号
 */
function escapeQuote(str) {
    if (!str) return '';
    return str.replace(/'/g, "\\'");
}

/**
 * 更新路径导航栏
 */
function updatePathNav() {
    const pathEl = document.getElementById('currentPath');
    if (!pathEl) return;

    const parts = currentPath.split('/').filter(p => p);

    let html = '';
    let buildPath = '';

    for (let i = 0; i < parts.length; i++) {
        if (i > 0) html += '<span> / </span>';
        buildPath += (i > 0 ? '/' : '') + parts[i];
        html += '<a href="#" onclick="navigateTo(\'' + escapeQuote(buildPath) + '\'); return false;">/' + escapeHtml(parts[i]) + '</a>';
    }

    pathEl.innerHTML = html;
}

// 初始化
function init() {
    // 从 URL 获取初始路径
    const params = new URLSearchParams(window.location.search);
    currentPath = params.get('path') || '';

    loadInfo();
    loadFiles();

    // 点击弹窗背景关闭
    const previewModal = document.getElementById('previewModal');
    if (previewModal) {
        previewModal.addEventListener('click', function(e) {
            if (e.target === this) {
                closePreview();
            }
        });
    }

    // ESC 键关闭预览/播放器
    document.addEventListener('keydown', function(e) {
        if (e.key === 'Escape') {
            closePreview();
            closePlayer();
            closeVideoPlayer();
        }
    });
}

// DOM 加载完成后初始化
if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', init);
} else {
    init();
}

// 监听浏览器后退/前进按钮
window.addEventListener('popstate', function(e) {
    if (e.state && e.state.path !== undefined) {
        currentPath = e.state.path;
    } else {
        const params = new URLSearchParams(window.location.search);
        currentPath = params.get('path') || '';
    }
    loadFiles();
});
