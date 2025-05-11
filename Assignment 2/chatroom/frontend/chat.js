// chatroom/frontend/chat.js
const ws = new WebSocket('ws://localhost:9002');
const sidebar = document.getElementById('sidebar');
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

// 当前聊天状态
let currentState = {
    targetType: 'room', // 'room', 'private', 'group'
    targetId: null,
    targetName: '大厅',
    selectedUser: null,
    selectedGroup: null,
    groups: [],
    onlineUsers: []
};

let membersRequestPending = false;   // 是否已向服务器请求成员列表但尚未收到

// 更新连接状态
function updateConnectionStatus(status) {
    connectionStatus.textContent = status;
    connectionStatus.className = status === '已连接' ? 'connected' : 'disconnected';
}

// 初始化侧边栏标签切换
sidebarTabs.forEach(tab => {
    tab.addEventListener('click', () => {
        // 取消所有标签和内容的活动状态
        sidebarTabs.forEach(t => t.classList.remove('active'));
        document.getElementById('user-list').classList.remove('active');
        document.getElementById('group-list').classList.remove('active');

        // 设置当前标签和对应内容为活动状态
        tab.classList.add('active');
        const tabTarget = tab.getAttribute('data-tab');
        document.getElementById(tabTarget).classList.add('active');
    });
});

// 初始化模态框
function initModals() {
    // 关闭所有模态框
    closeButtons.forEach(button => {
        button.addEventListener('click', () => {
            createGroupModal.style.display = 'none';
            manageGroupModal.style.display = 'none';
        });
    });

    // 点击模态框外部关闭
    window.addEventListener('click', event => {
        if (event.target === createGroupModal) {
            createGroupModal.style.display = 'none';
        }
        if (event.target === manageGroupModal) {
            manageGroupModal.style.display = 'none';
        }
    });

    // 创建群组按钮
    createGroupBtn.addEventListener('click', () => {
        document.getElementById('group-name-input').value = '';
        createGroupModal.style.display = 'block';
    });

    // 确认创建群组
    document.getElementById('create-group-confirm').addEventListener('click', () => {
        const groupName = document.getElementById('group-name-input').value.trim();
        if (groupName) {
            // 发送创建群组请求
            const createGroupRequest = {
                type: 'create_group',
                group_name: groupName
            };
            ws.send(JSON.stringify(createGroupRequest));
            createGroupModal.style.display = 'none';
        }
    });

    // 添加成员按钮
    document.getElementById('add-member-btn').addEventListener('click', () => {
        const username = document.getElementById('new-member-input').value.trim();
        if (username && currentState.selectedGroup) {
            // 发送添加成员请求
            const addMemberRequest = {
                type: 'add_group_member',
                group_id: currentState.selectedGroup.id,
                username: username
            };
            ws.send(JSON.stringify(addMemberRequest));
            document.getElementById('new-member-input').value = '';
        }
    });
}

// WebSocket事件处理
ws.addEventListener('open', () => {
    updateConnectionStatus('已连接');
    appendMessage('系统：已连接到服务器', 'system-msg');
});

ws.addEventListener('message', event => {
    const msg = event.data;

    try {
        // 尝试解析JSON消息
        const jsonMsg = JSON.parse(msg);

        if (jsonMsg.type === 'users_list') {
            // 处理用户列表
            updateUsersList(jsonMsg.users);
        }
        else if (jsonMsg.type === 'groups_list') {
            // 处理群组列表
            updateGroupsList(jsonMsg.groups);
        }
        else if (jsonMsg.type === 'create_group_response') {
            // 处理创建群组响应
            appendMessage('系统: ' + jsonMsg.message, 'system-msg');
        }
        else if (jsonMsg.type === 'add_member_response' || jsonMsg.type === 'remove_member_response') {
            // 处理添加/移除成员响应
            appendMessage('系统: ' + jsonMsg.message, 'system-msg');
        }
        else if (jsonMsg.type === 'group_members') {
            // 处理群组成员列表
            updateGroupMembers(jsonMsg.group_id, jsonMsg.members);
        }
        else if (jsonMsg.type === 'group_messages') {
            // 处理群组消息历史
            displayGroupMessages(jsonMsg.group_id, jsonMsg.messages);
        }
        else if (jsonMsg.type === 'group_message') {
            // 处理新的群组消息
            handleGroupMessage(jsonMsg);
        }
        else if (jsonMsg.type === 'notification') {
            // 处理通知消息
            appendMessage('系统: ' + jsonMsg.message, 'system-msg');
        }
        return;
    } catch (e) {
        // 如果不是JSON，按普通消息处理
    }

    // 处理普通文本消息
    if (msg.startsWith('=== 最近')) {
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

            const type = historyLines[i].includes('(私)') ? 'private-msg' :
                historyLines[i].includes('[群:') ? 'group-msg' : 'user-msg';
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
        } else if (msg.includes('[群:')) {
            type = 'group-msg';
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

// 更新用户列表
function updateUsersList(users) {
    currentState.onlineUsers = users;

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
            // 取消之前选中的用户
            const previousSelected = userList.querySelector('.user.selected');
            if (previousSelected) {
                previousSelected.classList.remove('selected');
            }

            // 选中当前用户
            userDiv.classList.add('selected');
            currentState.targetType = 'private';
            currentState.targetName = user;
            currentState.selectedUser = user;
            currentState.selectedGroup = null;

            // 更新聊天目标显示
            chatTarget.textContent = user + " (私聊)";

            // 更新输入框提示
            input.placeholder = `给 ${user} 发送私信...`;

            // 清除群组选中状态
            const selectedGroup = groupList.querySelector('.group.selected');
            if (selectedGroup) {
                selectedGroup.classList.remove('selected');
            }
        });
        userList.appendChild(userDiv);
    });
}

// 更新群组列表
function updateGroupsList(groups) {
    currentState.groups = groups;

    // 保留标题和创建按钮
    const title = groupList.querySelector('h3');
    const actions = groupList.querySelector('.group-actions');
    groupList.innerHTML = '';
    groupList.appendChild(title);

    // 显示群组数量
    const groupCount = document.createElement('div');
    groupCount.classList.add('group-count');
    groupCount.textContent = `我加入了 ${groups.length} 个群组`;
    groupList.appendChild(groupCount);

    groups.forEach(group => {
        const groupDiv = document.createElement('div');
        groupDiv.classList.add('group');
        groupDiv.innerHTML = group.name;

        // 如果是群主，添加标识
        if (group.is_owner) {
            const ownerBadge = document.createElement('span');
            ownerBadge.classList.add('group-owner-badge');
            ownerBadge.textContent = '群主';
            groupDiv.appendChild(ownerBadge);
        }

        groupDiv.addEventListener('click', () => {
            // 取消之前选中的群组
            const previousSelected = groupList.querySelector('.group.selected');
            if (previousSelected) {
                previousSelected.classList.remove('selected');
            }

            // 选中当前群组
            groupDiv.classList.add('selected');
            currentState.targetType = 'group';
            currentState.targetId = group.id;
            currentState.targetName = group.name;
            currentState.selectedGroup = group;
            currentState.selectedUser = null;

            // 更新聊天目标显示
            chatTarget.textContent = group.name + " (群聊)";

            // 更新输入框提示
            input.placeholder = `在群组 ${group.name} 中发言...`;

            // 清除用户选中状态
            const selectedUser = userList.querySelector('.user.selected');
            if (selectedUser) {
                selectedUser.classList.remove('selected');
            }

            // // 获取群组成员列表
            // const getMembersRequest = {
            //     type: 'get_group_members',
            //     group_id: group.id
            // };
            // ws.send(JSON.stringify(getMembersRequest));

            // 获取群组消息历史
            const getMessagesRequest = {
                type: 'get_group_messages',
                group_id: group.id
            };
            ws.send(JSON.stringify(getMessagesRequest));
        });

        // 添加管理成员按钮（仅群主可见）
        if (group.is_owner) {
            const manageBtn = document.createElement('button');
            manageBtn.textContent = '管理成员';
            manageBtn.style.marginLeft = '10px';
            manageBtn.style.fontSize = '0.7em';
            manageBtn.style.padding = '2px 5px';

            manageBtn.addEventListener('click', (e) => {
                e.stopPropagation();                       // 阻止冒泡

                currentState.selectedGroup = group;        // 记住当前群
                document.querySelector('#manage-members-modal .modal-title').textContent =
                    `群组 "${group.name}" 成员管理`;

                // === 只在需要时才去服务器拉数据 =========================
                if (!membersRequestPending) {              // 避免并发重复
                    membersRequestPending = true;
                    ws.send(JSON.stringify({
                        type: 'get_group_members',
                        group_id: group.id
                    }));
                }
                // 打开弹窗
                manageGroupModal.style.display = 'block';
            });

            groupDiv.appendChild(manageBtn);
        }

        groupList.appendChild(groupDiv);
    });

    // 重新添加创建群组按钮区域
    groupList.appendChild(actions);
}

// 更新群组成员列表
function updateGroupMembers(groupId, members) {
    // 如果不是当前选中的群组，忽略
    if (currentState.selectedGroup && currentState.selectedGroup.id !== groupId) {
        return;
    }

    const membersList = document.getElementById('members-list');
    membersList.innerHTML = '';

    const seen = new Set();

    members.forEach(member => {
        if (seen.has(member.username)) return;   // 已出现，跳过
        seen.add(member.username);
        // ------------------ 原来创建 DOM 的代码 ------------------
        const memberDiv = document.createElement('div');
        memberDiv.classList.add('group-member');

        const nameSpan = document.createElement('span');
        nameSpan.classList.add('group-member-name');
        nameSpan.textContent = member.username;

        const roleSpan = document.createElement('span');
        roleSpan.classList.add('group-member-role');
        roleSpan.textContent = member.is_owner ? '群主' : '成员';

        memberDiv.appendChild(nameSpan);
        memberDiv.appendChild(roleSpan);

        // 如果当前用户是群主且要显示的成员不是群主，添加移除按钮
        if (currentState.selectedGroup &&
            currentState.selectedGroup.is_owner &&
            !member.is_owner) {

            const removeBtn = document.createElement('span');
            removeBtn.classList.add('remove-member');
            removeBtn.innerHTML = '&times;';
            removeBtn.title = '移除成员';

            removeBtn.addEventListener('click', () => {
                if (confirm(`确定要将 ${member.username} 移出群组吗？`)) {
                    const removeMemberRequest = {
                        type: 'remove_group_member',
                        group_id: currentState.selectedGroup.id,
                        username: member.username
                    };
                    ws.send(JSON.stringify(removeMemberRequest));
                }
            });

            memberDiv.appendChild(removeBtn);
        }

        membersList.appendChild(memberDiv);
    });

    // 只有群主可以添加成员
    const addMemberContainer = document.getElementById('add-member-container');
    if (currentState.selectedGroup && currentState.selectedGroup.is_owner) {
        addMemberContainer.style.display = 'block';
    } else {
        addMemberContainer.style.display = 'none';
    }

    // 标记已完成本次请求，允许下次再发
    membersRequestPending = false;
}

// 显示群组消息历史
function displayGroupMessages(groupId, messages) {
    // 如果不是当前选中的群组，忽略
    if (currentState.selectedGroup && currentState.selectedGroup.id !== groupId) {
        return;
    }

    // 清空消息容器
    messageContainer.innerHTML = '';

    // 创建历史消息标题
    const historyTitle = document.createElement('div');
    historyTitle.classList.add('history-title');
    historyTitle.textContent = `=== ${currentState.selectedGroup.name} 群组消息历史 ===`;
    messageContainer.appendChild(historyTitle);

    // 添加各条历史消息
    if (messages.length === 0) {
        appendMessage('暂无消息历史', 'system-msg');
    } else {
        messages.forEach(msg => {
            const formatted = `[${msg.timestamp}] ${msg.sender}: ${msg.message}`;
            appendMessage(formatted, 'group-msg');
        });
    }

    // 添加分隔线
    const separator = document.createElement('div');
    separator.classList.add('separator');
    separator.textContent = '=== 以上是历史消息 ===';
    messageContainer.appendChild(separator);
}

// 处理新的群组消息
function handleGroupMessage(message) {
    appendMessage(message.formatted_message, 'group-msg');
}

// 添加消息到聊天窗口
function appendMessage(msg, type = 'user-msg') {
    const div = document.createElement('div');
    div.classList.add('message', type);
    div.textContent = msg;
    messageContainer.appendChild(div);
    messageContainer.scrollTop = messageContainer.scrollHeight;
}

// 发送消息
function sendMessage() {
    if (!input.value.trim()) return;

    if (currentState.targetType === 'private' && currentState.selectedUser) {
        // 私聊消息
        ws.send('@' + currentState.selectedUser + ' ' + input.value.trim());
    }
    else if (currentState.targetType === 'group' && currentState.selectedGroup) {
        // 群组消息
        const groupMessage = {
            type: 'group_message',
            group_id: currentState.selectedGroup.id,
            content: input.value.trim()
        };
        ws.send(JSON.stringify(groupMessage));
    }
    else {
        // 公共聊天室消息
        ws.send(input.value.trim());
    }

    input.value = '';
}

// 事件监听
sendButton.addEventListener('click', sendMessage);

input.addEventListener('keydown', event => {
    if (event.key === 'Enter') {
        sendMessage();
    }
});

// 切换回公共聊天
chatTarget.addEventListener('click', () => {
    if (currentState.targetType !== 'room') {
        currentState.targetType = 'room';
        currentState.targetId = null;
        currentState.targetName = '大厅';
        currentState.selectedUser = null;
        currentState.selectedGroup = null;

        // 更新UI
        chatTarget.textContent = '大厅';
        input.placeholder = '输入消息...';

        // 清除选中状态
        const selectedUser = userList.querySelector('.user.selected');
        if (selectedUser) {
            selectedUser.classList.remove('selected');
        }

        const selectedGroup = groupList.querySelector('.group.selected');
        if (selectedGroup) {
            selectedGroup.classList.remove('selected');
        }

        // 清空消息容器，回到公共聊天
        messageContainer.innerHTML = '';
        appendMessage('系统: 已切换到公共聊天室', 'system-msg');
    }
});

// 初始化
initModals();