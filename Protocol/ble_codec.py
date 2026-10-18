"""Transport reassembly for TX notifications; decode the result as steadyview.Response.

Create one instance per connection. Reset on reconnect, unsubscribe, or timeout.
"""
class ResponseAssembler:
    def __init__(self):
        self.reset()

    def reset(self):
        self.transfer_id = None
        self.count = 0
        self.next_index = 0
        self.payload = bytearray()

    def feed(self, notification):
        frame = bytes(notification)
        if not 5 <= len(frame) <= 20 or frame[0] != 0xA1:
            self.reset()
            raise ValueError("Invalid response fragment")
        _, transfer_id, index, count = frame[:4]
        if not 1 <= count <= 32 or index >= count or (index < count - 1 and len(frame) != 20):
            self.reset()
            raise ValueError("Invalid response fragment dimensions")
        if index == 0:
            if self.transfer_id is not None:
                self.reset()
                raise ValueError("Previous response was incomplete")
            self.transfer_id = transfer_id
            self.count = count
        if (transfer_id != self.transfer_id or count != self.count or index != self.next_index):
            self.reset()
            raise ValueError("Missing, duplicate or out-of-order fragment")
        self.payload.extend(frame[4:])
        self.next_index += 1
        if self.next_index == count:
            result = bytes(self.payload)
            self.reset()
            return result
        return None
