FPS Multiplayer Survival game in unreal 4.27


Content folder served seperately for its size.

[Google Drive Link](https://drive.google.com/file/d/1YX6nl45-ECZWfJ1Lb_3IypGZYiceI3v7/view?usp=sharing)

# Source File Explanation

## Source/Components
게임 내부에서 유저가 키를 눌러 상호작용하는 Actor에 부착되는 InteractionComponent와 Actor 각각이 인벤토리를 가지도록 하는 InventoryComponent 구현
## Source/Item
게임 내부에서 사용되는 아이템의 Base Class와 이것을 상속하여 만들어진 총알, 방어구, 총기, 일회성 아이템 등의 구현
## Source/Player
캐릭터에 대한 플레이어의 컨트롤을 주관하는 PlayerController와 캐릭터의 행동 구현 및 아이템 장착 여부 구현
## Source/Weapon
총기의 메쉬, 반동, 탄창크기, 데미지 등을 주관하는 Weapon Class와 투척용 아이템의 메쉬 정보등을 가지고 잇는 ThrowableWeapon Class와 기본공격의 데미지 구별을 위한 MeleeDamage Class
## Source/Widget
상호작용가능한 물체에 커서를 올렸을 때나 인벤토리에서 아이템에 커서를 올렸을 때 나오는 Widget Blueprint의 기본 Class들
## Source/World
월드에 배치되는 물체들 (ex. 픽업 아이템, 전리품 상자, 랜덤 아이템 스폰)을 주관하는 Class들
