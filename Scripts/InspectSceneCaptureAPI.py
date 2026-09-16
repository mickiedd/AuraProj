import unreal
for name in ['SceneCapture2D','SceneCaptureComponent2D','TextureRenderTarget2D','RenderingLibrary','TextureRenderTargetFactoryNew']:
    cls = getattr(unreal, name, None)
    print('CLS', name, cls)
    if cls:
        for member in dir(cls):
            if any(tok in member.lower() for tok in ['capture','render','export','target','texture','location','rotation']):
                try:
                    print('MEM', member, getattr(cls, member).__doc__)
                except Exception as e:
                    print('MEM', member, e)
