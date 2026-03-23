const translations = {
    en: {
        appName: "Put-In-Pipe",
        sendFile: "Send file",
        receiveFile: "Receive file",
        yourName: "Your name",
        namePlaceholder: "Enter your name",
        createSession: "Create session",
        joinSession: "Join session",
        shareLink: "Share this link with receivers:",
        copyLink: "Copy",
        copied: "Copied!",
        selectFile: "Select file",
        uploading: "Uploading...",
        downloading: "Downloading...",
        finishUpload: "Finish upload",
        terminateSession: "Terminate session",
        kick: "Kick",
        sender: "Sender",
        receivers: "Receivers",
        online: "online",
        offline: "offline",
        noReceivers: "Waiting for receivers...",
        captchaTitle: "Enter captcha",
        captchaExpires: "Expires in",
        captchaSubmit: "Submit",
        seconds: "s",
        transferComplete: "Transfer complete",
        transferTimeout: "Session timed out",
        senderGone: "Sender disconnected",
        noReceiversEnd: "No receivers connected",
        downloadFile: "Download file",
        startOver: "Start over",
        error: "Error",
        serverLoad: "Sessions",
        users: "Users",
        encrypted: "End-to-end encrypted",
        bufferStatus: "Buffer",
        waitingForFile: "Waiting for file info...",
        chunks: "chunks",
        startTransfer: "Start transfer",
    },
    ru: {
        appName: "Put-In-Pipe",
        sendFile: "Отправить файл",
        receiveFile: "Получить файл",
        yourName: "Ваше имя",
        namePlaceholder: "Введите имя",
        createSession: "Создать сессию",
        joinSession: "Подключиться к сессии",
        shareLink: "Отправьте ссылку получателям:",
        copyLink: "Копировать",
        copied: "Скопировано!",
        selectFile: "Выберите файл",
        uploading: "Загрузка...",
        downloading: "Скачивание...",
        finishUpload: "Завершить загрузку",
        terminateSession: "Завершить сессию",
        kick: "Отключить",
        sender: "Отправитель",
        receivers: "Получатели",
        online: "в сети",
        offline: "не в сети",
        noReceivers: "Ожидание получателей...",
        captchaTitle: "Введите капчу",
        captchaExpires: "Истекает через",
        captchaSubmit: "Отправить",
        seconds: "с",
        transferComplete: "Передача завершена",
        transferTimeout: "Время сессии истекло",
        senderGone: "Отправитель отключился",
        noReceiversEnd: "Получатели не подключились",
        downloadFile: "Скачать файл",
        startOver: "Начать заново",
        error: "Ошибка",
        serverLoad: "Сессии",
        users: "Пользователи",
        encrypted: "Сквозное шифрование",
        bufferStatus: "Буфер",
        waitingForFile: "Ожидание информации о файле...",
        chunks: "чанков",
        startTransfer: "Начать передачу",
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
