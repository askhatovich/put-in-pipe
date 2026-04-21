const translations = {
    en: {
        sendFile: "Send file",
        shareLink: "Share this link with receivers:",
        copyLink: "Copy",
        copied: "Copied!",
        selectFile: "Select file",
        terminateSession: "Terminate session",
        sender: "Sender",
        receivers: "Receivers",
        noReceivers: "Waiting for receivers...",
        captchaTitle: "Enter captcha",
        captchaExpires: "Expires in",
        captchaSubmit: "Submit",
        seconds: "s",
        transferComplete: "Transfer complete",
        transferTimeout: "Session timed out",
        senderGone: "Sender disconnected",
        noReceiversEnd: "No receivers connected",
        downloadFile: "Save file",
        startOver: "Home",
        error: "Error",
        serverLoad: "Sessions",
        users: "Users",
        encrypted: "Streaming file transfer with end-to-end encryption",
        bufferStatus: "Buffer",
        waitingForFile: "Waiting for file info...",
        chunks: "chunks",
        startTransfer: "Stop waiting",
        pasteLink: "Or paste a receive link",
        pasteLinkPlaceholder: "Paste link here...",
        joinFromLink: "Join",
        showQR: "QR",
        sessionExpires: "Session expires in",
        freezeExpires: "Waiting for receivers",
        leaveSession: "Leave session",
        fsWarning: "File System Access API is not supported by your browser. Maximum received file size depends on available RAM",
        fsWarningDesktop: "try desktop app",
        finalizing: "Finalizing file. Please wait...",
        finalizingWarn: "Do not close this tab",
        minutes: "min",
        kicked: "You were removed from the session",
        autoDropFreeze: "Start transfer automatically when a receiver connects",
        autoDropFreezeHint: "Drops the initial wait as soon as the first chunk is picked up",
    },
    ru: {
        sendFile: "Отправить файл",
        shareLink: "Отправьте ссылку получателям:",
        copyLink: "Копировать",
        copied: "Скопировано!",
        selectFile: "Выберите файл",
        terminateSession: "Завершить сессию",
        sender: "Отправитель",
        receivers: "Получатели",
        noReceivers: "Ожидание получателей...",
        captchaTitle: "Введите капчу",
        captchaExpires: "Истекает через",
        captchaSubmit: "Отправить",
        seconds: "с",
        transferComplete: "Передача завершена",
        transferTimeout: "Время сессии истекло",
        senderGone: "Отправитель отключился",
        noReceiversEnd: "Получатели не подключились",
        downloadFile: "Сохранить файл",
        startOver: "На главную",
        error: "Ошибка",
        serverLoad: "Сессии",
        users: "Пользователи",
        encrypted: "Потоковая передача файлов со сквозным шифрованием",
        bufferStatus: "Буфер",
        waitingForFile: "Ожидание информации о файле...",
        chunks: "чанков",
        startTransfer: "Закончить ожидание",
        pasteLink: "Или вставьте ссылку для получения",
        pasteLinkPlaceholder: "Вставьте ссылку...",
        joinFromLink: "Подключиться",
        showQR: "QR",
        sessionExpires: "Сессия истекает через",
        freezeExpires: "Ожидание получателей",
        leaveSession: "Покинуть сессию",
        fsWarning: "File System Access API не поддерживается вашим браузером. Максимальный размер принимаемого файла зависит от объёма оперативной памяти",
        fsWarningDesktop: "попробуйте десктопное приложение",
        finalizing: "Финализация файла. Подождите...",
        finalizingWarn: "Не закрывайте вкладку",
        minutes: "мин",
        kicked: "Вы были удалены из сессии",
        autoDropFreeze: "Начать передачу автоматически, когда подключится получатель",
        autoDropFreezeHint: "Снимает стартовое ожидание, как только первый чанк будет получен",
    },
};

const funnyNames = {
    en: [
        'Panda', 'Unicorn', 'Kitten', 'Penguin', 'Raccoon',
        'Fox', 'Otter', 'Koala', 'Hamster', 'Bunny',
        'Duckling', 'Hedgehog', 'Axolotl', 'Capybara', 'Sloth',
        'Narwhal', 'Platypus', 'Quokka', 'Chinchilla', 'Alpaca',
        'Firefly', 'Pixel', 'Cookie', 'Waffle', 'Noodle',
        'Biscuit', 'Muffin', 'Sprout', 'Pebble', 'Bubble',
        'Nugget', 'Pickle', 'Doodle', 'Fuzzy', 'Snowy',
        'Cloudy', 'Sunny', 'Starlight', 'Moonbeam', 'Breeze',
        'Whisker', 'Marble', 'Zigzag', 'Spark', 'Comet',
        'Turbo', 'Glitch', 'Widget', 'Byte', 'Pixel',
    ],
    ru: [
        'Панда', 'Единорог', 'Котик', 'Пингвин', 'Енотик',
        'Лисичка', 'Выдра', 'Коала', 'Хомячок', 'Зайчик',
        'Утёнок', 'Ёжик', 'Аксолотль', 'Капибара', 'Ленивец',
        'Нарвал', 'Утконос', 'Квокка', 'Шиншилла', 'Альпака',
        'Светлячок', 'Пиксель', 'Печенька', 'Вафелька', 'Лапша',
        'Бисквит', 'Кексик', 'Росток', 'Камешек', 'Пузырёк',
        'Наггетс', 'Огурчик', 'Каракуля', 'Пушистик', 'Снежок',
        'Облачко', 'Солнышко', 'Звёздочка', 'Лучик', 'Ветерок',
        'Усик', 'Шарик', 'Зигзаг', 'Искорка', 'Комета',
        'Турбо', 'Глитч', 'Виджет', 'Байтик', 'Вайб-кодер',
    ],
};

export function randomName(langCode) {
    const names = funnyNames[langCode] || funnyNames.en;
    return names[Math.floor(Math.random() * names.length)];
}

const NAME_STORAGE_KEY = 'pip_username';

export function getSavedName() {
    try { return localStorage.getItem(NAME_STORAGE_KEY) || ''; } catch { return ''; }
}

export function saveName(name) {
    try { localStorage.setItem(NAME_STORAGE_KEY, name); } catch { /* ignore */ }
}

export function getOrGenerateName() {
    const saved = getSavedName();
    if (saved) return saved;
    const generated = randomName(detectLang());
    saveName(generated);
    return generated;
}

const AUTO_DROP_FREEZE_KEY = 'pip_auto_drop_freeze';

export function getAutoDropFreeze() {
    try { return localStorage.getItem(AUTO_DROP_FREEZE_KEY) === '1'; } catch { return false; }
}

export function setAutoDropFreeze(enabled) {
    try { localStorage.setItem(AUTO_DROP_FREEZE_KEY, enabled ? '1' : '0'); } catch { /* ignore */ }
}

function detectLang() {
    if (typeof navigator === 'undefined') return 'en';
    const nav = navigator.language || navigator.userLanguage || '';
    return nav.startsWith('ru') ? 'ru' : 'en';
}

let currentLang = detectLang();
const listeners = new Set();

export { translations };

export function lang() {
    return currentLang;
}

export function setLang(l) {
    currentLang = l;
    listeners.forEach(fn => fn());
}

export function subscribe(fn) {
    listeners.add(fn);
    return () => listeners.delete(fn);
}

export function t(key) {
    return translations[currentLang]?.[key] ?? translations.en[key] ?? key;
}

export { detectLang };
