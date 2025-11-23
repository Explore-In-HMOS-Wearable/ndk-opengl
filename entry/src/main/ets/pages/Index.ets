import { window } from '@kit.ArkUI';
import CommonConstants from '../constants/CommonContants';
import nativeEntry from 'libentry.so';

const uiContext: UIContext | undefined = AppStorage.get('uiContext');
let context = uiContext!.getHostContext()!;

@Entry
@Component
struct Index {
  @State finalScore: number = 0
  @State gameOver: boolean = false
  @State isPlaying: boolean = false
  private xComponentController: XComponentController = new XComponentController()
  private xComponentContext: ESObject | undefined = undefined
  private renderContext?: ESObject;
  @State offsetX: number = 0;
  @State positionX: number = 0;
  private panOption: PanGestureOptions = new PanGestureOptions({
    direction: PanDirection.Horizontal,
    fingers: 1,
    distance: 3
  });
  private lastMoveTime: number = 0
  private moveThrottleMs: number = 5
  private accumulatedOffset: number = 0
  private moveMultiplier: number = 15
  private lastProcessedOffset: number = 0

  showGameOverDialog() {
    AlertDialog.show({
      title: 'Game Over!',
      message: `Your Score: ${this.finalScore}`,
      autoCancel: true,
      alignment: DialogAlignment.Center,
      offset: { dx: 0, dy: 0 },
      maskRect: {
        x: 0,
        y: 0,
        width: '100%',
        height: '100%'
      },
      confirm: {
        value: 'Restart',
        action: () => {
          this.RestartGame();
        }
      },
      cancel: () => {
        this.RestartGame();
      }
    });
  }

  private RestartGame() {
    this.gameOver = false;
    this.finalScore = 0;
    this.isPlaying = true;

    if (this.renderContext) {
      nativeEntry.restartGame(this.renderContext);
    }
  }

  async aboutToAppear(): Promise<void> {
    if (this.renderContext) {
      nativeEntry.setGameOverCallback(this.renderContext, (finalScore: number) => {
        console.info(`Game Over! Score: ${finalScore}`);
        this.finalScore = finalScore;
        this.gameOver = true;
        this.showGameOverDialog();
      });
    }

    let windowHeight: window.Window = await window.getLastWindow(context);
    await windowHeight.setWindowLayoutFullScreen(true);
    let sysBarProps: window.SystemBarProperties = {
      isStatusBarLightIcon: true
    };
    await windowHeight.setWindowSystemBarProperties(sysBarProps);
  }

  build() {
    Stack({ alignContent: Alignment.Top }) {

      if (this.isPlaying) {
        XComponent({
          id: 'A',
          type: XComponentType.SURFACE,
          libraryname: 'entry',
          controller: this.xComponentController
        })
          .id('A')
          .width('90%')
          .height('90%')
          .onLoad((xComponentContext) => {
            this.renderContext = xComponentContext;
            this.xComponentContext = xComponentContext;

            nativeEntry.setGameOverCallback(xComponentContext, (finalScore: number) => {
              this.finalScore = finalScore;
              this.gameOver = true;
              this.showGameOverDialog();
            });
          })
      }

      Column() {
        Row() {
          Text('Space Dodge')
            .fontSize(12)
            .fontWeight(900)
            .fontColor(Color.White)
            .margin({ top: '5%' })
        }

        Row() {
          if (!this.isPlaying && !this.gameOver) {
            Column() {
              Text('TAP TO START')
                .fontSize(28)
                .fontWeight(700)
                .fontColor('#ffff00')

              Text('Swipe left/right to dodge')
                .fontSize(14)
                .fontColor('#ffffff88')
                .margin({ top: 10 })
            }
            .width('100%')
            .height('100%')
            .justifyContent(FlexAlign.Center)
            .onClick(() => {
              this.isPlaying = true
            })
          }
        }
        .width(CommonConstants.FULL_PARENT)
        .layoutWeight(1)

        Row() {
          Text('Powered By OpenGL ES')
            .fontSize(8)
            .fontColor('#ff961717')
            .fontWeight(700)
        }
        .height('21vp')
      }
      .width('100%')
      .justifyContent(FlexAlign.Center)
    }
    .width(CommonConstants.FULL_PARENT)
    .height(CommonConstants.FULL_PARENT)
    .backgroundColor('#FF0A0A19')
    .gesture(
      PanGesture(this.panOption)
        .onActionStart((event: GestureEvent) => {
          this.lastMoveTime = 0
          this.accumulatedOffset = 0
          this.lastProcessedOffset = 0
        })
        .onActionUpdate((event: GestureEvent) => {
          if (!this.isPlaying || this.gameOver || !event || !this.xComponentContext) {
            return
          }

          const currentTime = Date.now()
          const timeSinceLastMove = currentTime - this.lastMoveTime

          const deltaOffset = event.offsetX - this.lastProcessedOffset
          this.lastProcessedOffset = event.offsetX

          this.accumulatedOffset += deltaOffset

          const absAccumulated = Math.abs(this.accumulatedOffset)

          if (absAccumulated >= this.moveMultiplier) {
            const steps = Math.floor(absAccumulated / this.moveMultiplier)

            for (let i = 0; i < steps * 3; i++) {
              if (this.accumulatedOffset > 0) {
                nativeEntry.moveRight(this.xComponentContext)
              } else {
                nativeEntry.moveLeft(this.xComponentContext)
              }
            }

            this.accumulatedOffset = this.accumulatedOffset % this.moveMultiplier
            this.lastMoveTime = currentTime
          }
        })
        .onActionEnd((event: GestureEvent) => {
        })
    )
  }
}