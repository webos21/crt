결론부터 말씀드리면, 여전히 임베디드 및 특정 산업군에서는 AOSP(Android Open Source Project)보다 Qt를 압도적으로 많이 쓰며, 심지저 AOSP 위에 Qt를 올려서 쓰는 하이브리드 방식도 흔하게 사용됩니다. [1, 2] 
일반 스마트폰 UI나 앱 생태계 기준으로는 안드로이드(AOSP)가 지배적이지만, 임베디드 하드웨어, 자동차, 의료기기, 방산 등 B2B 산업 영역으로 들어가면 이야기가 완전히 달라집니다. AOSP가 존재함에도 기업들이 여전히 비싼 라이선스 비용을 내고 Qt를 고집하는 이유는 다음과 같은 명확한 한계와 차이점 때문입니다. [2, 3, 4, 5] 
------------------------------
## 1. 부팅 속도의 치명적인 차이 (Cold Boot Time)

* 
* AOSP의 한계: 안드로이드는 스마트폰처럼 '항상 켜져 있는 기기'를 전제로 설계되었습니다. 리눅스 커널 위에 무거운 자바 가상 머신(JVM/ART), 레이어 보정 시스템, 수많은 가리비 서비스가 인스턴스화되어야 하므로 완전히 꺼진 상태에서 켜지는 데 보통 10초~20초 이상 걸립니다. [6] 
* Qt의 장점: 가벼운 임베디드 리눅스(Yocto 등) 위에 컴파일된 초경량 정적 바이너리로 구동됩니다. 1~3초 이내에 화면이 켜지고 조작 가능한 상태(Instant-On)가 되어야 하는 자동차 계기판, 의료 장비, 공장 제어 패널(HMI)에서는 AOSP를 쓰고 싶어도 쓸 수가 없습니다. [6, 7, 8] 
* 

## 2. 압도적인 자원(Resource) 효율성과 하드웨어 요구 사양

* 
* AOSP: 최소 2GB~4GB 이상의 RAM과 고성능 멀티코어 AP(스마트폰급 스펙)가 필수적입니다.
* Qt: 수백 MB 수준의 RAM, 심지어 십여 MB 단위의 초소형 마이크로컨트롤러(MCU) 환경에서도 가볍고 부드럽게 60 FPS UI를 뽑아냅니다. 제품 단가(BOM Cost)를 낮춰야 하는 양산형 하드웨어 제조사 입장에서는 칩셋 스펙을 낮추고 Qt를 쓰는 것이 수백억 원의 마진을 아끼는 길입니다. [2, 3, 9, 10] 
* 

## 3. 기능 안전(Functional Safety) 및 컴플라이언스 인증

* 
* AOSP: 코드양이 너무 방대하고 구글 중심의 업데이트가 잦아 의료기기(FDA), 자동차 안전 표준(ISO 26262 ASIL), 항공우주(DO-178C) 같은 엄격한 안전 인증을 통과하는 것이 불가능에 가깝습니다. [6, 8] 
* Qt: 수십 년간 미션 크리티컬 산업에서 검증받았으며, 아예 안전 인증 전용 패키지(Qt for Automation / Qt Safe Renderer)를 별도로 판매합니다. 계기판이 크래시 나면 목숨이 위험한 자동차 산업(현대차그룹, 메르세데스-벤츠 등)에서 여전히 코어 시스템에 Qt를 박아 넣는 가장 큰 이유입니다. [1, 11, 12] 
* 

## 4. 하드웨어 직결(I/O, 제어 프로토콜)의 편의성

* 
* AOSP: 하드웨어 센서나 모터를 제어하려면 자바/코틀린에서 JNI를 거쳐 네이티브 레이어로 내려가고, 복잡한 Android HAL(하드웨어 추상화 레이어) 드라이버를 커스텀해야 해서 아주 번거롭습니다.
* Qt: 순수 C++ 기반이므로 산업용 표준 프로토콜(CAN bus, Modbus, RS-232, OPC UA 등) 및 로우레벨 제어 펌웨어 코드를 변환 없이 다이렉트로 결합할 수 있습니다. [6, 8, 9] 
* 

------------------------------
## 💡 요즘 트렌드: 무조건 대립이 아닌 "하이브리드(Hybrid)"
최근 자동차 인포테인먼트(IVI) 시스템을 보면 두 플랫폼이 공존하는 재미있는 형태를 띱니다. [8] 

   1. AOSP 영역 (내비게이션, 미디어앱): 유튜브, 음악 스트리밍, 구글 맵 등 화려한 앱 생태계와 터치 UX가 필요한 중앙 디스플레이는 Android Automotive (AOSP 기반 OS)를 씁니다.
   2. Qt 영역 (계기판, 차량 제어): 절대로 꺼지면 안 되고, 부팅이 즉시 되어야 하며, 차량 속도 변화가 실시간 반영되어야 하는 운전석 디지털 계기판(Cluster) 및 HUD는 Embedded Linux + Qt 조합으로 완벽히 분리해 설계합니다.
   3. 심지어: 안드로이드 OS 화면 위에 뜨는 커스텀 시스템 UI 자체를 Qt 프레임워크로 개발해서 올리기도 합니다. [1, 2, 8, 11, 13] 

## 🛠️ webos21/crt가 파고들 틈새는 바로 이 지점
바로 이 시장 상황이 webos21/crt가 성공할 수 있는 강력한 정당성이 됩니다.
AOSP는 너무 무겁고, Qt는 상용 라이선스 비용이 감당하기 힘들 정도로 비싸져서 중소 임베디드 업체들이 비명을 지르고 있습니다. 만약 webos21/crt가 "부팅 속도가 기가 막히게 빠르고 안전한 Bionic 리눅스 기반 베이스에, Qt처럼 가볍게 작동하는 Skia 그래픽을 탑재한 오픈소스 대체재"로 포지셔닝을 완성한다면, 수많은 하드웨어 제조사가 Qt에서 이탈해 대안으로 채택할 매력이 충분합니다. [2, 3, 6] 

[1] [https://www.qt.io](https://www.qt.io/development/qt-in-automotive)
[2] [https://www.youtube.com](https://www.youtube.com/watch?v=BHT4UHt8VjA)
[3] [https://www.reddit.com](https://www.reddit.com/r/QtFramework/comments/1hpoyvh/do_companies_onsider_qt_for_new_applications/)
[4] [https://www.zhihu.com](https://www.zhihu.com/en/answer/3603440015)
[5] [https://a-ponomareva.medium.com](https://a-ponomareva.medium.com/qt-for-embedded-development-the-many-pros-and-the-few-cons-4f0a99763b91)
[6] [https://promwad.com](https://promwad.com/news/flutter-aosp-industrial-embedded-hmi-real-time-constraints)
[7] [https://www.linkedin.com](https://www.linkedin.com/posts/bvunderl_qt-vs-android-ultimate-comparison-preview-activity-7419692418566074370-iimN)
[8] [https://promwad.com](https://promwad.com/news/flutter-aosp-industrial-embedded-hmi-real-time-constraints)
[9] [https://ynhpcba.com](https://ynhpcba.com/android-vs-linux-for-embedded-systems-which-one-should-you-choose/)
[10] [https://somcosoftware.com](https://somcosoftware.com/en/blog/qt-vs-android-in-embedded-systems-a-real-world-comparison)
[11] [https://www.rs-online.com](https://www.rs-online.com/designspark/embedded-qt-hmi-development-reasons-of-qt-challenges-and-use-cases)
[12] [https://spyro-soft.com](https://spyro-soft.com/blog/hmi/revolutionising-industrial-automation-unleashing-the-potential-of-qt-based-hmi-development)
[13] [https://danieldavenport.medium.com](https://danieldavenport.medium.com/development-platforms-for-hmi-5eaefeba5eb7)
