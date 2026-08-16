-- BotCommander.lua
-- 机器人控制面板 (GM版 + 玩家版)

local BOT_COMMANDS = {
    SET = "/bot set",
    REMOVE = "/bot remove",
    LIST = "/bot list",
    MASTER = "/bot master",
    CLEARMASTER = "/bot clearmaster",
    STANCE = "/bot stance"
}

-- 姿态映射
local STANCE = {
    DEFENSIVE = "defensive",
    PASSIVE = "passive",
    AGGRESSIVE = "aggressive"
}

-- 检测是否为 GM
local function IsGM()
    -- 方法1: 直接检测（推荐）
    if UnitIsGM("player") then
        return true
    end
    -- 方法2: 通过特权命令检测（备用）
    -- 如果开启了 GM 模式但 UnitIsGM 返回 false，可以尝试发送一个无害的 GM 命令
    -- 这里不实际执行，只做检测
    return false
end

-- 判断当前用户角色
local userRole = IsGM() and "GM" or "PLAYER"
print("|cff00ff00[BotCommander]|r 当前用户模式: " .. userRole)

-- 主框架
local frame = CreateFrame("Frame", "BotCommanderFrame", UIParent)
frame:SetSize(350, 320)
frame:SetPoint("CENTER", 0, 0)
frame:SetBackdrop({
    bgFile = "Interface/Tooltips/UI-Tooltip-Background",
    edgeFile = "Interface/Tooltips/UI-Tooltip-Border",
    tile = true,
    tileSize = 16,
    edgeSize = 16,
    insets = { left = 8, right = 8, top = 8, bottom = 8 }
})
frame:SetBackdropColor(0, 0, 0, 0.85)
frame:SetMovable(true)
frame:EnableMouse(true)
frame:RegisterForDrag("LeftButton")
frame:SetScript("OnDragStart", function(self) self:StartMoving() end)
frame:SetScript("OnDragStop", function(self) self:StopMovingOrSizing() end)

-- 标题
local title = frame:CreateFontString(nil, "OVERLAY", "GameFontNormal")
title:SetPoint("TOP", 0, -10)
local titleText = IsGM() and "|cffff0000[GM]|r 机器人控制面板" or "|cff00ff00机器人控制面板|r"
title:SetText(titleText)

-- 机器人名输入框（GM专属）
local nameInput = nil
if IsGM() then
    nameInput = CreateFrame("EditBox", nil, frame, "InputBoxTemplate")
    nameInput:SetSize(120, 25)
    nameInput:SetPoint("TOPLEFT", 10, -45)
    nameInput:SetAutoFocus(false)
    nameInput:SetText("机器人名")
    nameInput:SetScript("OnEscapePressed", function(self) self:ClearFocus() end)
end

-- 姿态按钮
local function CreateStanceButton(parent, text, stance, yOffset)
    local btn = CreateFrame("Button", nil, parent, "UIPanelButtonTemplate")
    btn:SetSize(80, 30)
    btn:SetPoint("TOP", 0, yOffset)
    btn:SetText(text)
    btn:SetScript("OnClick", function()
        local name = nameInput and nameInput:GetText() or ""
        local cmd
        if IsGM() and name ~= "" and name ~= "机器人名" then
            cmd = BOT_COMMANDS.STANCE .. " " .. name .. " " .. stance
        else
            cmd = BOT_COMMANDS.STANCE .. " " .. stance
        end
        SendChatMessage(cmd, "SAY")
        print("|cff00ff00[BotCommander]|r 已发送: " .. cmd)
    end)
    return btn
end

-- 姿态按钮排列
local btnDefensive = CreateStanceButton(frame, "防御", STANCE.DEFENSIVE, -40)
local btnPassive = CreateStanceButton(frame, "被动", STANCE.PASSIVE, -80)
local btnAggressive = CreateStanceButton(frame, "攻击", STANCE.AGGESSIVE, -120)

-- 通用功能按钮
local function CreateActionButton(parent, text, command, yOffset)
    local btn = CreateFrame("Button", nil, parent, "UIPanelButtonTemplate")
    btn:SetSize(130, 25)
    btn:SetPoint("TOP", 0, yOffset)
    btn:SetText(text)
    btn:SetScript("OnClick", function()
        SendChatMessage(command, "SAY")
        print("|cff00ff00[BotCommander]|r 已发送: " .. command)
    end)
    return btn
end

-- ===== 玩家版功能 =====
local btnList = CreateActionButton(frame, "列出机器人", BOT_COMMANDS.LIST, -170)
local btnClearMaster = CreateActionButton(frame, "清除主人", BOT_COMMANDS.CLEARMASTER .. " " .. UnitName("player"), -200)

-- ===== GM 专属功能 =====
if IsGM() then
    -- 添加GM专用标签
    local gmLabel = frame:CreateFontString(nil, "OVERLAY", "GameFontNormalSmall")
    gmLabel:SetPoint("TOPLEFT", 140, -48)
    gmLabel:SetText("|cffff8888[GM专用]|r")
    gmLabel:SetTextColor(1, 0.5, 0.5)

    -- 设置机器人按钮
    local btnSet = CreateFrame("Button", nil, frame, "UIPanelButtonTemplate")
    btnSet:SetSize(80, 25)
    btnSet:SetPoint("TOPLEFT", 10, -80)
    btnSet:SetText("设为机器人")
    btnSet:SetScript("OnClick", function()
        local name = nameInput:GetText()
        if name and name ~= "" and name ~= "机器人名" then
            SendChatMessage(BOT_COMMANDS.SET .. " " .. name, "SAY")
            print("|cff00ff00[BotCommander]|r 已发送: " .. BOT_COMMANDS.SET .. " " .. name)
        else
            print("|cffff0000[BotCommander]|r 请输入有效的机器人名")
        end
    end)

    -- 移除机器人按钮
    local btnRemove = CreateFrame("Button", nil, frame, "UIPanelButtonTemplate")
    btnRemove:SetSize(80, 25)
    btnRemove:SetPoint("TOPLEFT", 100, -80)
    btnRemove:SetText("移除机器人")
    btnRemove:SetScript("OnClick", function()
        local name = nameInput:GetText()
        if name and name ~= "" and name ~= "机器人名" then
            SendChatMessage(BOT_COMMANDS.REMOVE .. " " .. name, "SAY")
            print("|cff00ff00[BotCommander]|r 已发送: " .. BOT_COMMANDS.REMOVE .. " " .. name)
        else
            print("|cffff0000[BotCommander]|r 请输入有效的机器人名")
        end
    end)

    -- 查看主人按钮
    local btnMaster = CreateFrame("Button", nil, frame, "UIPanelButtonTemplate")
    btnMaster:SetSize(80, 25)
    btnMaster:SetPoint("TOPLEFT", 190, -80)
    btnMaster:SetText("查看主人")
    btnMaster:SetScript("OnClick", function()
        local name = nameInput:GetText()
        if name and name ~= "" and name ~= "机器人名" then
            SendChatMessage(BOT_COMMANDS.MASTER .. " " .. name, "SAY")
            print("|cff00ff00[BotCommander]|r 已发送: " .. BOT_COMMANDS.MASTER .. " " .. name)
        else
            print("|cffff0000[BotCommander]|r 请输入有效的机器人名")
        end
    end)

    -- 清除指定机器人主人按钮
    local btnClearMasterSpecific = CreateFrame("Button", nil, frame, "UIPanelButtonTemplate")
    btnClearMasterSpecific:SetSize(130, 25)
    btnClearMasterSpecific:SetPoint("TOP", 0, -240)
    btnClearMasterSpecific:SetText("清除指定主人")
    btnClearMasterSpecific:SetScript("OnClick", function()
        local name = nameInput:GetText()
        if name and name ~= "" and name ~= "机器人名" then
            SendChatMessage(BOT_COMMANDS.CLEARMASTER .. " " .. name, "SAY")
            print("|cff00ff00[BotCommander]|r 已发送: " .. BOT_COMMANDS.CLEARMASTER .. " " .. name)
        else
            print("|cffff0000[BotCommander]|r 请输入有效的机器人名")
        end
    end)
end

-- 关闭按钮
local btnClose = CreateFrame("Button", nil, frame, "UIPanelButtonTemplate")
btnClose:SetSize(60, 20)
btnClose:SetPoint("BOTTOMRIGHT", -10, 10)
btnClose:SetText("关闭")
btnClose:SetScript("OnClick", function()
    frame:Hide()
end)

-- 显示/隐藏切换
local function ToggleFrame()
    if frame:IsShown() then
        frame:Hide()
    else
        frame:Show()
    end
end

-- Slash命令: /bc
SLASH_BOTCOMMANDER1 = "/bc"
SlashCmdList["BOTCOMMANDER"] = ToggleFrame

-- 首次加载显示
frame:Show()