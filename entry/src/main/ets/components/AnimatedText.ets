import { curves } from '@kit.ArkUI';

@Builder
function AnimatedText(opacityValue: number, translateY: number) {
  Column() {
    Text('ASMR Simulations')
      .opacity(opacityValue)
      .translate({ y: translateY })
      .fontSize(30)
      .fontWeight(FontWeight.Bolder)
      .fontColor(Color.White)
      .textShadow({
        radius: 40,
        color: '#ff00fff8',
        offsetX: 0,
        offsetY: 0
      })
      .animation({
        curve: curves.initCurve(),
        iterations: 1,
        duration: 500,
        delay: 500,
        playMode: PlayMode.Alternate,
      })
      .onAppear(() => {
        opacityValue = 1;
        translateY = 0;
      })
  }
  .width('100%')
}
