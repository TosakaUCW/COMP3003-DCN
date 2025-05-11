// chatroom/frontend/chat.js – 完整版 2025-05-12
// =============================================================
// 依赖：后端 server.cpp 推送的 JSON 协议保持不变
// 功能：Telegram 风格气泡 + 头像颜色一致 + 自己消息蓝色
// =============================================================

/* ---------------- DOM & State ---------------- */
const ws = new WebSocket('ws://localhost:9002');
const userList = document.getElementById('user-list');
const groupList = document.getElementById('group-list');
const messageContainer = document.getElementById('message-container');
const input = document.getElementById('input');
const sendButton = document.getElementById('send-button');
const connectionStatus = document.getElementById('connection-status');
const chatTarget = document.getElementById('chat-target');
const sidebarTabs = document.querySelectorAll('.sidebar-tab');
const createGroupBtn = document.getElementById('create-group-btn');
const createGroupModal = document.getElementById('create-group-modal');
const manageGroupModal = document.getElementById('manage-members-modal');
const closeButtons = document.querySelectorAll('.close-button, .close-modal');

let myUsername = '';       // 登录成功后写入
let membersRequestPending = false;    // 群成员请求节流
const avatarColors = new Map();// username -> hsl()

const currentState = {
    targetType: 'room', // room / private / group
    targetId: null,
    targetName: '大厅',
    selectedUser: null,
    selectedGroup: null,
    groups: [],
    onlineUsers: []
};

/* ========== 工具 ========= */
function updateConnectionStatus(text) {
    connectionStatus.textContent = text;
    connectionStatus.className = text === '已连接' ? 'connected' : 'disconnected';
}
function stringToColor(str) {
    let h = 0;
    for (let i = 0; i < str.length; i++) h = str.charCodeAt(i) + ((h << 5) - h);
    h %= 360; if (h < 0) h += 360;
    return `hsl(${h},70%,60%)`;
}
function getAvatarColor(user) {
    if (!avatarColors.has(user))
        avatarColors.set(user, stringToColor(user));
    return avatarColors.get(user);
}
/* 判断自己消息（公共 / 群 / 私） */
function isSelfMessage(raw) {
    if (!myUsername) return false;
    return (
        raw.includes(`] ${myUsername} :`) ||
        raw.includes(`] ${myUsername}:`) ||
        raw.includes(`] ${myUsername} (私)`)
    );
}

/* ======== 消息渲染（头像 + 气泡） ======== */
function appendMessage(raw, type = 'user-msg') {
    const outgoing = (type === 'self-msg' || isSelfMessage(raw));
    const row = document.createElement('div');
    row.className = 'msg-row ' + (outgoing ? 'outgoing' : 'incoming');

    /* 解析发送者用户名 */
    let sender = '';
    if (raw.includes('(私) 对 ')) {
        sender = raw.match(/\] (.*?) \(私\)/)?.[1] || '';
    } else {
        sender = raw.match(/\] (.*?)[:\(]/)?.[1] || '';
    }
    if (outgoing) sender = myUsername || sender;

    /* 头像 */
    const avatar = document.createElement('div');
    avatar.className = 'avatar';
    avatar.textContent = sender.charAt(0).toUpperCase();
    avatar.style.background = getAvatarColor(sender);

    /* 气泡：去掉前缀时间戳，保持内容 */
    const bubble = document.createElement('div');
    bubble.className = 'bubble';
    bubble.textContent = raw.replace(/^\[.*?\]\s*/, '');

    /* 组装：自己消息头像在右，别人消息在左 */
    if (outgoing) {
        row.appendChild(bubble);
        row.appendChild(avatar);
    } else {
        row.appendChild(avatar);
        row.appendChild(bubble);
    }

    messageContainer.appendChild(row);
    messageContainer.scrollTop = messageContainer.scrollHeight;
}

/* ======== 历史记录渲染 ======== */
function displayHistory(arr) {
    if (!arr || !arr.length) return;
    const title = document.createElement('div');
    title.className = 'history-title';
    title.textContent = '=== 最近消息历史 ===';
    messageContainer.appendChild(title);

    for (let i = arr.length - 1; i >= 0; i--) {
        appendMessage(arr[i].raw);
    }
    const sep = document.createElement('div');
    sep.className = 'separator';
    sep.textContent = '=== 以上是历史消息 ===';
    messageContainer.appendChild(sep);
}

/* ======== 侧边栏 Tab 切换 ======== */
sidebarTabs.forEach(tab => {
    tab.addEventListener('click', () => {
        sidebarTabs.forEach(t => t.classList.remove('active'));
        document.getElementById('user-list').classList.remove('active');
        document.getElementById('group-list').classList.remove('active');
        tab.classList.add('active');
        document.getElementById(tab.dataset.tab).classList.add('active');
    });
});

/* ======== 模态框 ======== */
function initModals() {
    closeButtons.forEach(btn => {
        btn.addEventListener('click', () => { createGroupModal.style.display = 'none'; manageGroupModal.style.display = 'none'; });
    });
    window.addEventListener('click', e => {
        if (e.target === createGroupModal) createGroupModal.style.display = 'none';
        if (e.target === manageGroupModal) manageGroupModal.style.display = 'none';
    });
    createGroupBtn.addEventListener('click', () => {
        document.getElementById('group-name-input').value = '';
        createGroupModal.style.display = 'block';
    });
    document.getElementById('create-group-confirm').addEventListener('click', () => {
        const name = document.getElementById('group-name-input').value.trim();
        if (name) {
            ws.send(JSON.stringify({ type: 'create_group', group_name: name }));
            createGroupModal.style.display = 'none';
        }
    });
    document.getElementById('add-member-btn').addEventListener('click', () => {
        const u = document.getElementById('new-member-input').value.trim();
        if (u && currentState.selectedGroup) {
            ws.send(JSON.stringify({ type: 'add_group_member', group_id: currentState.selectedGroup.id, username: u }));
            document.getElementById('new-member-input').value = '';
        }
    });
}

/* ======== WebSocket ======== */
ws.addEventListener('open', () => { updateConnectionStatus('已连接'); appendMessage('系统：已连接到服务器', 'system-msg'); });
ws.addEventListener('close', () => { updateConnectionStatus('已断开'); appendMessage('系统：连接已关闭', 'system-msg'); });
ws.addEventListener('error', () => { updateConnectionStatus('连接错误'); appendMessage('系统：连接发生错误', 'system-msg'); });

ws.addEventListener('message', event => {
    const msg = event.data;

    if (typeof msg === 'string' && msg.startsWith('系统: 登录成功，欢迎 ')) myUsername = msg.split('欢迎 ')[1].trim();

    try {
        const j = JSON.parse(msg);
        switch (j.type) {
            case 'users_list': updateUsersList(j.users); break;
            case 'groups_list': updateGroupsList(j.groups); break;
            case 'history': displayHistory(j.messages); break;
            case 'create_group_response':
            case 'add_member_response':
            case 'remove_member_response':
            case 'notification':
                appendMessage('系统: ' + j.message, 'system-msg'); break;
            case 'group_members': updateGroupMembers(j.group_id, j.members); break;
            case 'group_messages': displayGroupMessages(j.group_id, j.messages); break;
            case 'group_message': handleGroupMessage(j); break;
        }
        return;
    } catch { }

    appendMessage(msg, isSelfMessage(msg) ? 'self-msg' : 'user-msg');
});

/* ======== 用户列表 ======== */
function updateUsersList(users) {
    currentState.onlineUsers = users;
    const title = userList.querySelector('h3');
    userList.innerHTML = ''; userList.appendChild(title);

    const cnt = document.createElement('div');
    cnt.className = 'user-count'; cnt.textContent = `当前共 ${users.length} 人在线`;
    userList.appendChild(cnt);

    users.forEach(u => {
        const d = document.createElement('div');
        d.className = 'user'; d.textContent = u;
        d.addEventListener('click', () => {
            userList.querySelectorAll('.user.selected').forEach(el => el.classList.remove('selected'));
            d.classList.add('selected');
            Object.assign(currentState, { targetType: 'private', targetName: u, selectedUser: u, selectedGroup: null });
            chatTarget.textContent = u + ' (私聊)'; input.placeholder = `给 ${u} 发送私信...`;
            groupList.querySelectorAll('.group.selected').forEach(el => el.classList.remove('selected'));
        });
        userList.appendChild(d);
    });
}

/* ======== 群组列表 ======== */
function updateGroupsList(groups) {
    currentState.groups = groups;
    const title = groupList.querySelector('h3');
    const actions = groupList.querySelector('.group-actions');
    groupList.innerHTML = ''; groupList.appendChild(title);

    const cnt = document.createElement('div');
    cnt.className = 'group-count'; cnt.textContent = `我加入了 ${groups.length} 个群组`;
    groupList.appendChild(cnt);

    groups.forEach(g => {
        const d = document.createElement('div');
        d.className = 'group'; d.textContent = g.name;
        if (g.is_owner) {
            const badge = document.createElement('span');
            badge.className = 'group-owner-badge'; badge.textContent = '群主';
            d.appendChild(badge);
        }
        d.addEventListener('click', () => {
            groupList.querySelectorAll('.group.selected').forEach(el => el.classList.remove('selected'));
            d.classList.add('selected');
            Object.assign(currentState, { targetType: 'group', targetId: g.id, targetName: g.name, selectedGroup: g, selectedUser: null });
            chatTarget.textContent = g.name + ' (群聊)';
            input.placeholder = `在群组 ${g.name} 中发言...`;
            userList.querySelectorAll('.user.selected').forEach(el => el.classList.remove('selected'));
            ws.send(JSON.stringify({ type: 'get_group_messages', group_id: g.id }));
        });

        if (g.is_owner) {
            const btn = document.createElement('button');
            btn.textContent = '管理成员'; btn.style.cssText = 'margin-left:10px;font-size:.7em;padding:2px 5px';
            btn.addEventListener('click', e => {
                e.stopPropagation();
                currentState.selectedGroup = g;
                if (!membersRequestPending) {
                    membersRequestPending = true;
                    ws.send(JSON.stringify({ type: 'get_group_members', group_id: g.id }));
                }
                document.querySelector('#manage-members-modal .modal-title').textContent = `群组 "${g.name}" 成员管理`;
                manageGroupModal.style.display = 'block';
            });
            d.appendChild(btn);
        }
        groupList.appendChild(d);
    });
    groupList.appendChild(actions);
}

/* ======== 群组成员渲染 ======== */
function updateGroupMembers(gid, members) {
    if (currentState.selectedGroup && currentState.selectedGroup.id !== gid) return;
    const list = document.getElementById('members-list');
    list.innerHTML = '';
    const seen = new Set();
    members.forEach(m => {
        if (seen.has(m.username)) return; seen.add(m.username);
        const div = document.createElement('div'); div.className = 'group-member';
        const name = document.createElement('span'); name.className = 'group-member-name'; name.textContent = m.username;
        const role = document.createElement('span'); role.className = 'group-member-role'; role.textContent = m.is_owner ? '群主' : '成员';
        div.appendChild(name); div.appendChild(role);
        if (currentState.selectedGroup && currentState.selectedGroup.is_owner && !m.is_owner) {
            const del = document.createElement('span'); del.className = 'remove-member'; del.innerHTML = '&times;';
            del.addEventListener('click', () => {
                if (confirm(`确定要将 ${m.username} 移出群组吗？`))
                    ws.send(JSON.stringify({ type: 'remove_group_member', group_id: currentState.selectedGroup.id, username: m.username }));
            });
            div.appendChild(del);
        }
        list.appendChild(div);
    });
    document.getElementById('add-member-container').style.display =
        (currentState.selectedGroup && currentState.selectedGroup.is_owner) ? 'block' : 'none';
    membersRequestPending = false;
}

/* ======== 群组历史 / 新消息 ======== */
function displayGroupMessages(gid, msgs) {
    if (currentState.selectedGroup && currentState.selectedGroup.id !== gid) return;
    messageContainer.innerHTML = '';
    const title = document.createElement('div');
    title.className = 'history-title';
    title.textContent = `=== ${currentState.selectedGroup.name} 群组消息历史 ===`;
    messageContainer.appendChild(title);

    if (!msgs.length) appendMessage('暂无消息历史', 'system-msg');
    else msgs.forEach(m => {
        const raw = `[${m.timestamp}] ${m.sender}: ${m.message}`;
        appendMessage(raw, m.sender === myUsername ? 'self-msg' : 'group-msg');
    });
    const sep = document.createElement('div'); sep.className = 'separator'; sep.textContent = '=== 以上是历史消息 ===';
    messageContainer.appendChild(sep);
}
function handleGroupMessage(j) {
    const raw = j.formatted_message || `[${j.timestamp}] ${j.sender}: ${j.content}`;
    appendMessage(raw, j.sender === myUsername ? 'self-msg' : 'group-msg');
}

/* ======== 发送消息 ======== */
function sendMessage() {
    const text = input.value.trim(); if (!text) return;
    if (currentState.targetType === 'private' && currentState.selectedUser)
        ws.send('@' + currentState.selectedUser + ' ' + text);
    else if (currentState.targetType === 'group' && currentState.selectedGroup)
        ws.send(JSON.stringify({ type: 'group_message', group_id: currentState.selectedGroup.id, content: text }));
    else ws.send(text);
    input.value = '';
}
sendButton.addEventListener('click', sendMessage);
input.addEventListener('keydown', e => { if (e.key === 'Enter') sendMessage(); });

/* ======== 切回大厅 ======== */
chatTarget.addEventListener('click', () => {
    if (currentState.targetType !== 'room') {
        Object.assign(currentState, { targetType: 'room', targetId: null, targetName: '大厅', selectedUser: null, selectedGroup: null });
        chatTarget.textContent = '大厅'; input.placeholder = '输入消息...';
        userList.querySelectorAll('.user.selected').forEach(el => el.classList.remove('selected'));
        groupList.querySelectorAll('.group.selected').forEach(el => el.classList.remove('selected'));
        messageContainer.innerHTML = ''; appendMessage('系统: 已切换到公共聊天室', 'system-msg');
    }
});

/* ======== 初始化 ======== */
initModals();
