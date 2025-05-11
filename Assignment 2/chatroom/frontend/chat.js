// chatroom/frontend/chat.js – Telegram-style 完整版
// ===================================================
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

let myUsername = '';                 // 登录成功后写入
let membersRequestPending = false;   // 成员列表请求节流

/* ---------------- 当前状态 ---------------- */
let currentState = {
    targetType: 'room',   // room / private / group
    targetId: null,
    targetName: '大厅',
    selectedUser: null,
    selectedGroup: null,
    groups: [],
    onlineUsers: []
};

/* ---------------- 工具函数 ---------------- */
function updateConnectionStatus(text) {
    connectionStatus.textContent = text;
    connectionStatus.className = text === '已连接' ? 'connected' : 'disconnected';
}

function isSelfMessage(raw) {
    if (!myUsername) return false;

    return (
        raw.includes(`] ${myUsername} :`) ||   // 公共：有空格冒号
        raw.includes(`] ${myUsername}:`) ||   // 群聊：紧贴冒号
        raw.includes(`] ${myUsername} (私)`)// 私聊
    );
}


function appendMessage(text, type = 'user-msg') {
    /* 1. 判断方向（自己 or 其他） */
    const isOutgoing = (type === 'self-msg');
    const row = document.createElement('div');
    row.classList.add('msg-row');
    row.classList.add(isOutgoing ? 'outgoing' : 'incoming');

    /* 2. 头像：首字母占位 + 颜色 hash */
    const avatar = document.createElement('div');
    avatar.className = 'avatar';
    let name = currentState.targetType === 'private' && isOutgoing ? myUsername :
        (isOutgoing ? myUsername : text.match(/\] (.*?)[:\(]/)?.[1] || '?');
    avatar.textContent = name.charAt(0).toUpperCase();
    avatar.style.background = stringToColor(name);

    /* 3. 气泡 */
    const bubble = document.createElement('div');
    bubble.className = 'bubble';
    bubble.textContent = text.replace(/^\[.*?\] /, '');   // 去掉时间戳前缀

    /* 4. 拼装 */
    if (isOutgoing) {
        row.appendChild(bubble);
        row.appendChild(avatar);      // 自己的头像在右侧
    } else {
        row.appendChild(avatar);
        row.appendChild(bubble);
    }
    messageContainer.appendChild(row);
    messageContainer.scrollTop = messageContainer.scrollHeight;
}

function stringToColor(str) {
    let hash = 0;
    for (let i = 0; i < str.length; i++) hash = str.charCodeAt(i) + ((hash << 5) - hash);
    const h = hash % 360;
    return `hsl(${h},70%,60%)`;
}

function displayHistory(messages) {
    if (!messages || messages.length === 0) return;

    // 标题
    const title = document.createElement('div');
    title.className = 'history-title';
    title.textContent = '=== 最近消息历史 ===';
    messageContainer.appendChild(title);

    // 服务器按 id DESC，所以倒序显示
    for (let i = messages.length - 1; i >= 0; i--) {
        const line = messages[i].raw;
        let type = 'user-msg';
        if (line.includes('(私)')) type = 'private-msg';
        else if (line.includes('[群:')) type = 'group-msg';
        if (isSelfMessage(line)) type = 'self-msg';
        appendMessage(line, type);
    }

    // 分隔线
    const sep = document.createElement('div');
    sep.className = 'separator';
    sep.textContent = '=== 以上是历史消息 ===';
    messageContainer.appendChild(sep);
}

/* ---------------- 侧边栏 Tab ---------------- */
sidebarTabs.forEach(tab => {
    tab.addEventListener('click', () => {
        sidebarTabs.forEach(t => t.classList.remove('active'));
        document.getElementById('user-list').classList.remove('active');
        document.getElementById('group-list').classList.remove('active');
        tab.classList.add('active');
        document.getElementById(tab.dataset.tab).classList.add('active');
    });
});

/* ---------------- 模态框初始化 ---------------- */
function initModals() {
    closeButtons.forEach(btn => {
        btn.addEventListener('click', () => {
            createGroupModal.style.display = 'none';
            manageGroupModal.style.display = 'none';
        });
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
            ws.send(JSON.stringify({
                type: 'add_group_member',
                group_id: currentState.selectedGroup.id,
                username: u
            }));
            document.getElementById('new-member-input').value = '';
        }
    });
}

/* ---------------- WebSocket 事件 ---------------- */
ws.addEventListener('open', () => {
    updateConnectionStatus('已连接');
    appendMessage('系统：已连接到服务器', 'system-msg');
});

ws.addEventListener('message', event => {
    const msg = event.data;

    // 捕获登录成功欢迎语获得我的用户名
    if (typeof msg === 'string' && msg.startsWith('系统: 登录成功，欢迎 '))
        myUsername = msg.split('欢迎 ')[1].trim();

    // -------- JSON 消息分支 --------
    try {
        const j = JSON.parse(msg);
        switch (j.type) {
            case 'users_list': updateUsersList(j.users); break;
            case 'groups_list': updateGroupsList(j.groups); break;
            case 'history': displayHistory(j.messages); break;
            case 'create_group_response':
                appendMessage('系统: ' + j.message, 'system-msg'); break;
            case 'add_member_response':
            case 'remove_member_response':
                appendMessage('系统: ' + j.message, 'system-msg'); break;
            case 'group_members': updateGroupMembers(j.group_id, j.members); break;
            case 'group_messages': displayGroupMessages(j.group_id, j.messages); break;
            case 'group_message': handleGroupMessage(j); break;
            case 'notification': appendMessage('系统: ' + j.message, 'system-msg'); break;
        }
        return;
    } catch { }

    // -------- 普通文本消息分支 --------
    let type = 'user-msg';
    if (msg.startsWith('系统:')) type = 'system-msg';
    else if (msg.includes('(私)')) type = 'private-msg';
    else if (msg.includes('[群:')) type = 'group-msg';
    if (isSelfMessage(msg)) type = 'self-msg';
    appendMessage(msg, type);
});

ws.addEventListener('close', () => {
    updateConnectionStatus('已断开');
    appendMessage('系统：连接已关闭', 'system-msg');
});
ws.addEventListener('error', () => {
    updateConnectionStatus('连接错误');
    appendMessage('系统：连接发生错误', 'system-msg');
});

/* ---------------- 在线用户列表 ---------------- */
function updateUsersList(users) {
    currentState.onlineUsers = users;

    const title = userList.querySelector('h3');
    userList.innerHTML = '';
    userList.appendChild(title);

    const count = document.createElement('div');
    count.className = 'user-count';
    count.textContent = `当前共 ${users.length} 人在线`;
    userList.appendChild(count);

    users.forEach(u => {
        const d = document.createElement('div');
        d.className = 'user';
        d.textContent = u;
        d.addEventListener('click', () => {
            userList.querySelectorAll('.user.selected').forEach(el => el.classList.remove('selected'));
            d.classList.add('selected');
            currentState = {
                ...currentState,
                targetType: 'private', targetName: u, selectedUser: u, selectedGroup: null
            };
            chatTarget.textContent = u + ' (私聊)';
            input.placeholder = `给 ${u} 发送私信...`;
            groupList.querySelectorAll('.group.selected').forEach(el => el.classList.remove('selected'));
        });
        userList.appendChild(d);
    });
}

/* ---------------- 群组列表 ---------------- */
function updateGroupsList(groups) {
    currentState.groups = groups;

    const title = groupList.querySelector('h3');
    const actions = groupList.querySelector('.group-actions');
    groupList.innerHTML = '';
    groupList.appendChild(title);

    const cnt = document.createElement('div');
    cnt.className = 'group-count';
    cnt.textContent = `我加入了 ${groups.length} 个群组`;
    groupList.appendChild(cnt);

    groups.forEach(g => {
        const d = document.createElement('div');
        d.className = 'group';
        d.textContent = g.name;

        if (g.is_owner) {
            const badge = document.createElement('span');
            badge.className = 'group-owner-badge';
            badge.textContent = '群主';
            d.appendChild(badge);
        }

        d.addEventListener('click', () => {
            groupList.querySelectorAll('.group.selected').forEach(el => el.classList.remove('selected'));
            d.classList.add('selected');
            currentState = {
                ...currentState,
                targetType: 'group', targetId: g.id, targetName: g.name,
                selectedGroup: g, selectedUser: null
            };
            chatTarget.textContent = g.name + ' (群聊)';
            input.placeholder = `在群组 ${g.name} 中发言...`;
            userList.querySelectorAll('.user.selected').forEach(el => el.classList.remove('selected'));

            // 拉取群消息历史
            ws.send(JSON.stringify({ type: 'get_group_messages', group_id: g.id }));
        });

        // 管理成员按钮（群主可见）
        if (g.is_owner) {
            const btn = document.createElement('button');
            btn.textContent = '管理成员';
            btn.style.cssText = 'margin-left:10px;font-size:0.7em;padding:2px 5px';
            btn.addEventListener('click', e => {
                e.stopPropagation();
                currentState.selectedGroup = g;

                // 如果不是在等待中，拉一次成员列表
                if (!membersRequestPending) {
                    membersRequestPending = true;
                    ws.send(JSON.stringify({ type: 'get_group_members', group_id: g.id }));
                }

                document.querySelector('#manage-members-modal .modal-title').textContent =
                    `群组 "${g.name}" 成员管理`;
                manageGroupModal.style.display = 'block';
            });
            d.appendChild(btn);
        }

        groupList.appendChild(d);
    });

    groupList.appendChild(actions);
}

/* ---------------- 群组成员 ---------------- */
function updateGroupMembers(gid, members) {
    if (currentState.selectedGroup && currentState.selectedGroup.id !== gid) return;

    const membersList = document.getElementById('members-list');
    membersList.innerHTML = '';

    const seen = new Set();  // 去重
    members.forEach(m => {
        if (seen.has(m.username)) return;
        seen.add(m.username);

        const div = document.createElement('div');
        div.className = 'group-member';

        const name = document.createElement('span');
        name.className = 'group-member-name';
        name.textContent = m.username;

        const role = document.createElement('span');
        role.className = 'group-member-role';
        role.textContent = m.is_owner ? '群主' : '成员';

        div.appendChild(name); div.appendChild(role);

        if (currentState.selectedGroup && currentState.selectedGroup.is_owner && !m.is_owner) {
            const del = document.createElement('span');
            del.className = 'remove-member';
            del.innerHTML = '&times;';
            del.title = '移除成员';
            del.addEventListener('click', () => {
                if (confirm(`确定要将 ${m.username} 移出群组吗？`)) {
                    ws.send(JSON.stringify({
                        type: 'remove_group_member',
                        group_id: currentState.selectedGroup.id,
                        username: m.username
                    }));
                }
            });
            div.appendChild(del);
        }
        membersList.appendChild(div);
    });

    document.getElementById('add-member-container').style.display =
        (currentState.selectedGroup && currentState.selectedGroup.is_owner) ? 'block' : 'none';

    membersRequestPending = false;   // 本次请求完成
}

/* ---------------- 显示群消息历史 ---------------- */
function displayGroupMessages(gid, msgs) {
    if (currentState.selectedGroup && currentState.selectedGroup.id !== gid) return;

    messageContainer.innerHTML = '';
    const title = document.createElement('div');
    title.className = 'history-title';
    title.textContent = `=== ${currentState.selectedGroup.name} 群组消息历史 ===`;
    messageContainer.appendChild(title);

    if (msgs.length === 0) {
        appendMessage('暂无消息历史', 'system-msg');
    } else {
        msgs.forEach(m => {
            const line = `[${m.timestamp}] ${m.sender}: ${m.message}`;
            appendMessage(line, m.sender === myUsername ? 'self-msg' : 'group-msg');
        });
    }

    const sep = document.createElement('div');
    sep.className = 'separator';
    sep.textContent = '=== 以上是历史消息 ===';
    messageContainer.appendChild(sep);
}

function handleGroupMessage(j) {
    appendMessage(j.formatted_message, j.sender === myUsername ? 'self-msg' : 'group-msg');
}

/* ---------------- 发送消息 ---------------- */
function sendMessage() {
    const text = input.value.trim();
    if (!text) return;

    if (currentState.targetType === 'private' && currentState.selectedUser) {
        ws.send('@' + currentState.selectedUser + ' ' + text);
    } else if (currentState.targetType === 'group' && currentState.selectedGroup) {
        ws.send(JSON.stringify({ type: 'group_message', group_id: currentState.selectedGroup.id, content: text }));
    } else {
        ws.send(text);  // 公共
    }
    input.value = '';
}

sendButton.addEventListener('click', sendMessage);
input.addEventListener('keydown', e => { if (e.key === 'Enter') sendMessage(); });

/* ---------------- 点击“大厅”返回公共聊天 ---------------- */
chatTarget.addEventListener('click', () => {
    if (currentState.targetType !== 'room') {
        currentState = {
            ...currentState,
            targetType: 'room', targetId: null, targetName: '大厅',
            selectedUser: null, selectedGroup: null
        };
        chatTarget.textContent = '大厅';
        input.placeholder = '输入消息...';
        userList.querySelectorAll('.user.selected').forEach(el => el.classList.remove('selected'));
        groupList.querySelectorAll('.group.selected').forEach(el => el.classList.remove('selected'));
        messageContainer.innerHTML = '';
        appendMessage('系统: 已切换到公共聊天室', 'system-msg');
    }
});

/* 初始化模态框 */
initModals();
