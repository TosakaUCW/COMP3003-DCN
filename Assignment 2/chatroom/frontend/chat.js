// chatroom/frontend/chat.js
const ws = new WebSocket('ws://localhost:9002');
const userList = document.getElementById('user-list');
const messageContainer = document.getElementById('message-container');
const input = document.getElementById('input');
const sendButton = document.getElementById('send-button');
const connectionStatus = document.getElementById('connection-status');

// 更新连接状态
function updateConnectionStatus(status) {
    connectionStatus.textContent = status;
    connectionStatus.className = status === '已连接' ? 'connected' : 'disconnected';
}

ws.addEventListener('open', () => {
    updateConnectionStatus('已连接');
    appendMessage('系统：已连接到服务器', 'system-msg');
});

ws.addEventListener('message', event => {
    const msg = event.data;

    if (msg.startsWith('在线用户列表：')) {
        const users = msg.substring('在线用户列表：'.length).split(',');
        updateUserList(users);
    } 
    else if (msg.startsWith('=== 最近')) {
        // 历史消息处理
        const historyLines = msg.split('\n');
        
        // 创建历史消息标题
        const historyTitle = document.createElement('div');
        historyTitle.classList.add('history-title');
        historyTitle.textContent = historyLines[0];
        messageContainer.appendChild(historyTitle);
        
        // 添加各条历史消息
        for (let i = 1; i < historyLines.length - 1; i++) {
            if (historyLines[i] === '=== 历史消息结束 ===' || historyLines[i] === '') continue;
            
            const type = historyLines[i].includes('(私)') ? 'private-msg' : 'user-msg';
            appendMessage(historyLines[i], type);
        }
        
        // 添加分隔线
        const separator = document.createElement('div');
        separator.classList.add('separator');
        separator.textContent = '=== 以上是历史消息 ===';
        messageContainer.appendChild(separator);
    }
    else {
        // 普通消息处理
        let type = 'user-msg';
        
        if (msg.startsWith('系统:')) {
            type = 'system-msg';
        } else if (msg.includes('(私)')) {
            type = 'private-msg';
        }
        
        appendMessage(msg, type);
    }
});

ws.addEventListener('close', () => {
    updateConnectionStatus('已断开');
    appendMessage('系统：连接已关闭', 'system-msg');
});

ws.addEventListener('error', () => {
    updateConnectionStatus('连接错误');
    appendMessage('系统：连接发生错误', 'system-msg');
});

// 发送消息
function sendMessage() {
    if (input.value.trim()) {
        ws.send(input.value.trim());
        input.value = '';
    }
}

// 按钮发送
sendButton.addEventListener('click', sendMessage);

// 回车键发送
input.addEventListener('keydown', event => {
    if (event.key === 'Enter') {
        sendMessage();
    }
});

function appendMessage(msg, type = 'user-msg') {
    const div = document.createElement('div');
    div.classList.add('message', type);
    div.textContent = msg;
    messageContainer.appendChild(div);
    messageContainer.scrollTop = messageContainer.scrollHeight;
}

function updateUserList(users) {
    // 保留标题
    const title = userList.querySelector('h3');
    userList.innerHTML = '';
    userList.appendChild(title);

    // 显示用户数量
    const userCount = document.createElement('div');
    userCount.classList.add('user-count');
    userCount.textContent = `当前共 ${users.length} 人在线`;
    userList.appendChild(userCount);

    users.forEach(user => {
        const userDiv = document.createElement('div');
        userDiv.classList.add('user');
        userDiv.textContent = user;
        userDiv.addEventListener('click', () => {
            input.value = '@' + user + ' ';
            input.focus();
        });
        userList.appendChild(userDiv);
    });
}