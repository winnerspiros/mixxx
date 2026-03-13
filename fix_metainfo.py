with open('res/linux/org.mixxx.Mixxx.metainfo.xml', 'r') as f:
    content = f.read()

content = content.replace('https://opencollective.com/mixxx</url>', 'https://opencollective.com/mixxx/</url>')

with open('res/linux/org.mixxx.Mixxx.metainfo.xml', 'w') as f:
    f.write(content)
