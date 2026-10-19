#ifndef UNIHELL_GUI_LANG_H
#define UNIHELL_GUI_LANG_H

enum {
    GUI_S_TITLE,
    GUI_S_TX,
    GUI_S_STOP,
    GUI_S_RX,
    GUI_S_READY,
    GUI_S_MODE,
    GUI_S_FREQ,
    GUI_S_BW,
    GUI_S_INPUT,
    GUI_S_OUTPUT,
    GUI_S_SQL,
    GUI_S_WATERFALL,
    GUI_S_PHASE,
    GUI_S_RATE,
    GUI_S_LANGUAGE,
    GUI_S_AUTO,
    GUI_S_DEFAULT_DEVICE,
    GUI_S_DEVICE_FMT,
    GUI_S_STATUS_TX,
    GUI_S_STATUS_END,
    GUI_S_STATUS_STOPPED,
    GUI_S_STATUS_RECEIVING,
    GUI_S_STATUS_SQL_PAUSE,
    GUI_S_STATUS_INPUT_SELECTED,
    GUI_S_STATUS_OUTPUT_SELECTED,
    GUI_S_STATUS_SWITCH_INPUT,
    GUI_S_STATUS_RX_STOPPED,
    GUI_S_STATUS_CABLE_WARN,
    GUI_S_ERR_INPUT_FORMAT,
    GUI_S_ERR_OPEN_INPUT,
    GUI_S_STATUS_RX_RATE_FMT,
    GUI_S_COUNT
};

enum {
    GUI_LANG_ZH,
    GUI_LANG_EN,
    GUI_LANG_KO,
    GUI_LANG_JA,
    GUI_LANG_DE,
    GUI_LANG_RU,
    GUI_LANG_UK,
    GUI_LANG_IT,
    GUI_LANG_FR,
    GUI_LANG_COUNT
};

static const wchar_t *const gui_lang_codes[GUI_LANG_COUNT] = {
    L"zh", L"en", L"ko", L"ja", L"de", L"ru", L"uk", L"it", L"fr"
};

static const wchar_t *const gui_lang_names[GUI_LANG_COUNT] = {
    L"汉语", L"English", L"한국어", L"日本語", L"Deutsch",
    L"Русский", L"Українська", L"Italiano", L"Français"
};

static const wchar_t *const gui_text[GUI_LANG_COUNT][GUI_S_COUNT] = {
    {
        L"Hellschreiber 收发", L"发射", L"停止", L"接收",
        L"就绪", L"模式", L"频率", L"带宽", L"输入声卡", L"输出声卡",
        L"SQL", L"瀑布图", L"相位", L"速率", L"界面语言", L"自动调整",
        L"默认设备", L"设备 %d", L"发射中", L"发送结束标记", L"停止",
        L"接收中", L"SQL 暂停", L"输入设备已选择", L"输出设备已选择",
        L"切换输入设备", L"接收停止",
        L"警告：回环时需选择 CABLE Input",
        L"录音设备不支持可用格式", L"打开录音设备失败",
        L"接收中 %u Hz/%uch"
    },
    {
        L"Hellschreiber transceiver", L"Transmit", L"Stop", L"Receive",
        L"Ready", L"Mode", L"Frequency", L"Bandwidth", L"Input device",
        L"Output device", L"SQL", L"Waterfall", L"Phase", L"Rate",
        L"Language", L"Auto",
        L"Default device", L"Device %d", L"Transmitting",
        L"Sending end marker", L"Stopped", L"Receiving", L"SQL paused",
        L"Input device selected", L"Output device selected",
        L"Switching input device", L"Reception stopped",
        L"Warning: select CABLE Input for loopback",
        L"Recording device does not support an available format",
        L"Failed to open recording device", L"Receiving %u Hz/%uch"
    },
    {
        L"Hellschreiber 송수신", L"송신", L"정지", L"수신",
        L"준비", L"모드", L"주파수", L"대역폭", L"입력 장치",
        L"출력 장치", L"SQL", L"워터폴", L"위상", L"속도",
        L"인터페이스 언어", L"자동 조정",
        L"기본 장치", L"장치 %d", L"송신 중", L"종료 표시 전송 중",
        L"정지됨", L"수신 중", L"SQL 일시 중지", L"입력 장치 선택됨",
        L"출력 장치 선택됨", L"입력 장치 전환 중", L"수신 중지됨",
        L"경고: 루프백에는 CABLE Input을 선택하십시오",
        L"녹음 장치가 지원되는 형식을 지원하지 않습니다",
        L"녹음 장치를 열지 못했습니다", L"수신 중 %u Hz/%uch"
    },
    {
        L"Hellschreiber 送受信", L"送信", L"停止", L"受信",
        L"準備完了", L"モード", L"周波数", L"帯域幅", L"入力デバイス",
        L"出力デバイス", L"SQL", L"ウォーターフォール", L"位相", L"速度",
        L"表示言語", L"自動調整",
        L"既定のデバイス", L"デバイス %d", L"送信中", L"終了マーカー送信中",
        L"停止", L"受信中", L"SQL 一時停止", L"入力デバイスを選択しました",
        L"出力デバイスを選択しました", L"入力デバイスを切り替え中",
        L"受信停止",
        L"警告：ループバックには CABLE Input を選択してください",
        L"録音デバイスが利用可能な形式に対応していません",
        L"録音デバイスを開けませんでした", L"受信中 %u Hz/%uch"
    },
    {
        L"Hellschreiber-Sender/Empfänger", L"Senden", L"Stopp", L"Empfangen",
        L"Bereit", L"Modus", L"Frequenz", L"Bandbreite", L"Eingabegerät",
        L"Ausgabegerät", L"SQL", L"Wasserfall", L"Phase", L"Rate",
        L"Sprache", L"Automatisch",
        L"Standardgerät", L"Gerät %d", L"Sendet", L"Endmarke wird gesendet",
        L"Gestoppt", L"Empfängt", L"SQL pausiert", L"Eingabegerät ausgewählt",
        L"Ausgabegerät ausgewählt", L"Eingabegerät wird gewechselt",
        L"Empfang gestoppt",
        L"Warnung: Für Loopback CABLE Input wählen",
        L"Aufnahmegerät unterstützt kein verfügbares Format",
        L"Aufnahmegerät konnte nicht geöffnet werden",
        L"Empfängt %u Hz/%uch"
    },
    {
        L"Hellschreiber: приём и передача", L"Передача", L"Стоп", L"Приём",
        L"Готово", L"Режим", L"Част.", L"Полоса", L"Устройство ввода",
        L"Устройство вывода", L"SQL", L"Водопад", L"Фаза", L"Скорость",
        L"Язык", L"Авто",
        L"Устройство по умолчанию", L"Устройство %d", L"Передача",
        L"Отправка конечной метки", L"Остановлено", L"Приём", L"SQL пауза",
        L"Устройство ввода выбрано", L"Устройство вывода выбрано",
        L"Переключение устройства ввода", L"Приём остановлен",
        L"Внимание: для петли выберите CABLE Input",
        L"Устройство записи не поддерживает доступный формат",
        L"Не удалось открыть устройство записи",
        L"Приём %u Hz/%uch"
    },
    {
        L"Hellschreiber: приймання й передавання", L"Передати", L"Стоп",
        L"Прийом", L"Готово", L"Режим", L"Част.", L"Смуга",
        L"Пристрій введення", L"Пристрій виведення", L"SQL", L"Водоспад",
        L"Фаза", L"Швидкість", L"Мова", L"Авто",
        L"Пристрій за замовчуванням", L"Пристрій %d", L"Передавання",
        L"Надсилання кінцевої позначки", L"Зупинено", L"Приймання",
        L"SQL призупинено", L"Пристрій введення вибрано",
        L"Пристрій виведення вибрано", L"Перемикання пристрою введення",
        L"Приймання зупинено",
        L"Увага: для петлі виберіть CABLE Input",
        L"Пристрій запису не підтримує доступний формат",
        L"Не вдалося відкрити пристрій запису",
        L"Приймання %u Hz/%uch"
    },
    {
        L"Ricetrasmettitore Hellschreiber", L"Trasmetti", L"Arresta",
        L"Ricevi", L"Pronto", L"Modo", L"Frequenza", L"Banda",
        L"Dispositivo di ingresso", L"Dispositivo di uscita", L"SQL",
        L"Cascata", L"Fase", L"Velocità", L"Lingua", L"Automatico",
        L"Dispositivo predefinito", L"Dispositivo %d", L"Trasmissione",
        L"Invio marcatore finale", L"Arrestato", L"Ricezione", L"SQL in pausa",
        L"Dispositivo di ingresso selezionato",
        L"Dispositivo di uscita selezionato",
        L"Cambio dispositivo di ingresso", L"Ricezione arrestata",
        L"Avviso: per il loopback selezionare CABLE Input",
        L"Il dispositivo di registrazione non supporta un formato disponibile",
        L"Impossibile aprire il dispositivo di registrazione",
        L"Ricezione %u Hz/%uch"
    },
    {
        L"Émetteur-récepteur Hellschreiber", L"Émettre", L"Arrêter",
        L"Recevoir", L"Prêt", L"Mode", L"Fréquence", L"Bande",
        L"Périphérique d'entrée", L"Périphérique de sortie", L"SQL",
        L"Cascade", L"Phase", L"Vitesse", L"Langue", L"Automatique",
        L"Périphérique par défaut", L"Périphérique %d", L"Émission",
        L"Envoi du marqueur de fin", L"Arrêté", L"Réception",
        L"SQL en pause", L"Périphérique d'entrée sélectionné",
        L"Périphérique de sortie sélectionné",
        L"Changement de périphérique d'entrée", L"Réception arrêtée",
        L"Avertissement : sélectionnez CABLE Input pour la boucle",
        L"Le périphérique d'enregistrement ne prend pas en charge de format disponible",
        L"Impossible d'ouvrir le périphérique d'enregistrement",
        L"Réception %u Hz/%uch"
    }
};

#endif
