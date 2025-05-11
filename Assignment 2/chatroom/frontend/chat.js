// frontend/chat.js
const ws = new WebSocket('ws://localhost:9002');
const userList = document.getElementById('user-list');
const messageContainer = document.getElementById('message-container');
const input = document.getElementById('input');

ws.addEventListener('open', () => {
    appendMessage('系统：已连接到服务器', 'system-msg');
});

ws.addEventListener('message', event => {
    const msg = event.data;

    if (msg.startsWith('在线用户列表：')) {
        const users = msg.substring('在线用户列表：'.length).split(',');
        updateUserList(users);
    } else {
        appendMessage(msg);
    }
});

ws.addEventListener('close', () => {
    appendMessage('系统：连接已关闭', 'system-msg');
});

input.addEventListener('keydown', event => {
    if (event.key === 'Enter' && input.value.trim()) {
        ws.send(input.value.trim());
        input.value = '';
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
